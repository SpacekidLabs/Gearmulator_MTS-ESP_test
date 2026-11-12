#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>

namespace n2x
{
	// One publisher (device/audio thread), one reader (MCU thread). The reader
	// never waits: an overlapping publication is picked up on the next MCU cycle.
	class TuningMailbox
	{
	public:
		using Table = std::array<int32_t, 128>;
		static_assert(std::atomic<uint32_t>::is_always_lock_free);
		static_assert(std::atomic<int32_t>::is_always_lock_free);

		TuningMailbox()
		{
			for(auto& value : m_values)
				value.store(0);
		}

		void publish(const Table& _table)
		{
			if(_table == m_published)
				return;
			m_version.fetch_add(1);
			for(size_t i = 0; i < _table.size(); ++i)
				m_values[i].store(_table[i]);
			m_version.fetch_add(1);
			m_published = _table;
		}

		bool read(Table& _table, uint32_t& _version) const
		{
			const auto before = m_version.load();
			if((before & 1) || before == _version)
				return false;
			Table snapshot;
			for(size_t i = 0; i < snapshot.size(); ++i)
				snapshot[i] = m_values[i].load();
			if(m_version.load() != before)
				return false;
			_table = snapshot;
			_version = before;
			return true;
		}

		static int32_t targetOffset(const double _semitones)
		{
			// Reject corrupt input, and bound arithmetic well inside signed 32-bit.
			if(!std::isfinite(_semitones) || std::abs(_semitones) > 256.0)
				return 0;
			return static_cast<int32_t>(std::llround(_semitones * 16384.0));
		}

	private:
		std::array<std::atomic<int32_t>, 128> m_values;
		std::atomic<uint32_t> m_version{0};
		Table m_published{};
	};

	class VoiceTuning
	{
	public:
		static constexpr uint32_t VoiceBase = 0x1cb6;
		static constexpr uint32_t VoiceStride = 0x11a;
		static constexpr uint32_t VoiceCount = 20;

		static int voiceIndex(const uint32_t _a4, const uint32_t _address)
		{
			if(_a4 < 0x100000 || _a4 > 0x130000 || _address < _a4 + VoiceBase)
				return -1;
			const auto delta = _address - _a4 - VoiceBase;
			if(delta % VoiceStride || delta / VoiceStride >= VoiceCount)
				return -1;
			return static_cast<int>(delta / VoiceStride);
		}

		int32_t written(const int _voice, const uint16_t _note, const int32_t _base, const TuningMailbox::Table& _table)
		{
			auto& v = m_voices[static_cast<size_t>(_voice)];
			v.base = _base;
			v.note = _note;
			v.valid = true;
			v.lastTarget = add(_base, offset(_note, _table));
			return v.lastTarget;
		}

		int32_t refresh(const int _voice, const uint16_t _note, const int32_t _current, const TuningMailbox::Table& _table)
		{
			auto& v = m_voices[static_cast<size_t>(_voice)];
			// Unknown writes/voice changes must not reuse another note's baseline.
			if(!v.valid || v.note != _note || v.lastTarget != _current)
			{
				v.valid = false;
				return _current;
			}
			v.lastTarget = add(v.base, offset(_note, _table));
			return v.lastTarget;
		}

	private:
		static int32_t offset(const uint16_t _note, const TuningMailbox::Table& _table)
		{
			return _note < _table.size() ? _table[_note] : 0;
		}

		static int32_t add(const int32_t _base, const int32_t _offset)
		{
			const auto result = static_cast<int64_t>(_base) + _offset;
			if(result < std::numeric_limits<int32_t>::min() || result > std::numeric_limits<int32_t>::max())
				return _base;
			return static_cast<int32_t>(result);
		}

		struct Voice
		{
			int32_t base = 0;
			int32_t lastTarget = 0;
			uint16_t note = 0;
			bool valid = false;
		};
		std::array<Voice, VoiceCount> m_voices{};
	};
}
