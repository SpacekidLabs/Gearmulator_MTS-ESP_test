#pragma once
#include <array>
#include <atomic>
#include <cmath>
#include <algorithm>
#include <cstdint>
namespace dsp56k { class DSP; }
namespace virusLib
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
		static int32_t offset(const double _semitones, const int _note)
		{
			if(!std::isfinite(_semitones) || std::abs(_semitones) > 256.0) return 0;
			return static_cast<int32_t>(std::llround(std::clamp(_semitones, -double(_note), 127.0 - _note) * 4096.0));
		}
	private:
		std::array<std::atomic<int32_t>, 128> m_values;
		std::atomic<uint32_t> m_version{0};
		Table m_published{};
	};

	// Native pitch is 4096 units per semitone, after glide, before oscillator modulation.
	// Only installed for the SHA-256-verified TI2 firmware by Device.
	class VoiceTuning
	{
	public:
		bool install(dsp56k::DSP& _dsp);
		void publish(const TuningMailbox::Table& _table) { m_mailbox.publish(_table); }
		void refresh(dsp56k::DSP& _dsp);
	private:
		TuningMailbox m_mailbox;
		TuningMailbox::Table m_table{};
		uint32_t m_version = 0;
		bool m_installed = false;
	};
}
