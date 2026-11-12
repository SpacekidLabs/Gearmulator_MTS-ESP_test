#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <algorithm>

#include "cpu/h8300h/machine.hpp"

namespace jeLib
{
	// Audio thread publishes; the emulation thread takes a bounded snapshot.
	class TuningMailbox
	{
	public:
		using Table = std::array<int32_t, 128>;
		static_assert(std::atomic<int32_t>::is_always_lock_free);
		static_assert(std::atomic<uint32_t>::is_always_lock_free);
		TuningMailbox() { for(auto& v : m_values) v.store(0); }
		void publish(const Table& _table)
		{
			if(_table == m_published) return;
			m_version.fetch_add(1);
			for(size_t i = 0; i < _table.size(); ++i) m_values[i].store(_table[i]);
			m_version.fetch_add(1);
			m_published = _table;
		}
		bool read(Table& _table, uint32_t& _version) const
		{
			const auto before = m_version.load();
			if((before & 1) || before == _version) return false;
			Table snapshot;
			for(size_t i = 0; i < snapshot.size(); ++i) snapshot[i] = m_values[i].load();
			if(m_version.load() != before) return false;
			_table = snapshot;
			_version = before;
			return true;
		}
		static int32_t cents(const double _semitones)
		{
			if(!std::isfinite(_semitones) || std::abs(_semitones) > 256.0) return 0;
			return static_cast<int32_t>(std::llround(_semitones * 100.0));
		}
	private:
		std::array<std::atomic<int32_t>, 128> m_values;
		std::atomic<uint32_t> m_version{0};
		Table m_published{};
	};

	// JP-8000 v1.05: intercept the native base-pitch writes, before the
	// oscillator and glide calculations. Reads and all other writes stay RAM.
	// Installed only after an exact firmware digest match in Je8086.
	class VoiceTuning final : public emu::Device
	{
	public:
		static constexpr uint32_t VoiceBase = 0x2067da;
		static constexpr uint32_t VoiceStride = 0xf0;
		static constexpr uint32_t VoiceCount = 8;
		static constexpr uint32_t PitchOffset = 0x40;
		static constexpr uint32_t NoteOffset = 0x24; // original key, before patch transpose

		explicit VoiceTuning(h8300h::Machine& _machine) : m_machine(_machine)
		{
			auto& bus = m_machine.bus();
			constexpr uint32_t first = VoiceBase & ~127u;
			constexpr uint32_t end = (VoiceBase + VoiceCount * VoiceStride + 127) & ~127u;
			bus.map_rom_device(first, end - first, this, bus.attr(first) & h8300h::Bus::kClassMask);
		}

		void publish(const TuningMailbox::Table& _table) { m_mailbox.publish(_table); }
		void refresh()
		{
			if(!m_mailbox.read(m_table, m_version)) return;
			auto* mem = m_machine.bus().mem();
			for(size_t i = 0; i < m_voices.size(); ++i)
			{
				auto& v = m_voices[i];
				const auto base = VoiceBase + i * VoiceStride;
				if(!v.valid) continue;
				if(mem[base + NoteOffset] != v.note || h8300h::Bus::get16(mem + base + PitchOffset) != v.last)
				{
					v.valid = false;
					continue;
				}
				v.last = tuned(v.base, v.note);
				h8300h::Bus::put16(mem + base + PitchOffset, v.last);
			}
		}

		uint8_t read8(uint32_t _address) override { return m_machine.bus().mem()[_address]; }
		void write8(uint32_t _address, uint8_t _value) override
		{
			invalidate(_address);
			m_machine.bus().mem()[_address] = _value;
		}
		void write16(uint32_t _address, uint16_t _value) override
		{
			const auto pc = m_machine.cpu().current_pc();
			const int voice = pitchVoice(_address);
			if(voice >= 0 && (pc == 0xa846 || pc == 0xe8a2 || pc == 0xf2fc || pc == 0x19fda))
			{
				auto& v = m_voices[voice];
				v.base = static_cast<int16_t>(_value);
				v.note = m_machine.bus().mem()[_address - PitchOffset + NoteOffset];
				v.valid = true;
				_value = v.last = tuned(v.base, v.note);
			}
			else
			{
				invalidate(_address);
				invalidate(_address + 1);
			}
			h8300h::Bus::put16(m_machine.bus().mem() + _address, _value);
		}
	private:
		static int pitchVoice(uint32_t _address)
		{
			if(_address < VoiceBase + PitchOffset) return -1;
			const auto delta = _address - VoiceBase - PitchOffset;
			return delta % VoiceStride == 0 && delta / VoiceStride < VoiceCount ? int(delta / VoiceStride) : -1;
		}
		void invalidate(uint32_t _address)
		{
			const auto v = pitchVoice(_address & ~1u);
			if(v >= 0) m_voices[v].valid = false;
		}
		uint16_t tuned(int16_t _base, uint8_t _note) const
		{
			const int offset = _note < m_table.size() ? m_table[_note] : 0;
			return static_cast<uint16_t>(std::clamp(int(_base) + offset, -32768, 32767));
		}
		struct Voice { int16_t base = 0; uint16_t last = 0; uint8_t note = 0; bool valid = false; };
		h8300h::Machine& m_machine;
		TuningMailbox m_mailbox;
		TuningMailbox::Table m_table{};
		uint32_t m_version = 0;
		std::array<Voice, VoiceCount> m_voices{};
	};
}
