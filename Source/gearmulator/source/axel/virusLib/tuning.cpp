#include "tuning.h"

#include "dsp56kEmu/assembler.h"
#include "dsp56kEmu/dsp.h"

#include <stdexcept>
#include <vector>

namespace virusLib
{
	// Unused bridged memory in the verified firmware. P/X share this region;
	// the table, saved registers and program therefore have separate ranges.
	static constexpr unsigned Table = 0x30000;
	static constexpr unsigned Trampoline = 0x30200;

	static void installHook(dsp56k::DSP& _dsp)
	{
		auto& memory = _dsp.memory();
		if(memory.get(dsp56k::MemArea_P, 0x774) != 0x205800 ||
			memory.get(dsp56k::MemArea_P, 0x775) != 0x43d800)
			throw std::runtime_error("pitch signature mismatch");
		for(unsigned address = Table; address < 0x30300; ++address)
			if(memory.get(dsp56k::MemArea_X, address) || memory.get(dsp56k::MemArea_P, address))
				throw std::runtime_error("tuning memory occupied");

		// Assemble and validate everything before replacing any native instruction.
		std::vector<std::pair<unsigned, unsigned>> program;
		dsp56k::Assembler assembler;
		unsigned pc = Trampoline;
		const auto emit = [&](const char* _instruction)
		{
			const auto result = assembler.assemble(_instruction);
			if(!result.success()) throw std::runtime_error("invalid tuning instruction");
			for(unsigned i = 0; i < result.wordCount; ++i)
				program.emplace_back(pc++, result.word[i]);
		};

		// P:$773 has already saved the unmodified glide state. Reproduce the two
		// replaced instructions, then add the current note's offset only to A.
		// Native bend, oscillator tuning and modulation run afterwards.
		for(const auto* instruction : {
			"move (r0)+", "move l:(r0)+,y",
			"move b2,x:>$30103", "move b1,x:>$30104", "move b0,x:>$30105",
			"move r2,x:>$30100", "move x0,x:>$30101", "move sr,x:>$30102",
			// Original MIDI key (before patch transpose), packed into X:$base+2e.
			"move x:(r0+$5),b", "asr #$10,b,b", "and #>$7f,b",
			"add #>$30000,b", "move b1,r2", "move x:(r2),x0", "add x0,a",
			"move x:>$30103,b2", "move x:>$30104,b1", "move x:>$30105,b0",
			"move x:>$30101,x0", "move x:>$30100,r2", "move x:>$30102,sr", "rts"})
			emit(instruction);

		if(pc >= 0x30300) throw std::runtime_error("tuning trampoline overflow");
		for(const auto& [address, word] : program) _dsp.memWriteP(address, word);
		_dsp.memWriteP(0x774, 0x0bf080); // JSR absolute, two words
		_dsp.memWriteP(0x775, Trampoline);
	}

	bool VoiceTuning::install(dsp56k::DSP& _dsp)
	{
		try
		{
			installHook(_dsp);
			m_installed = true;
			return true;
		}
		catch(const std::exception&)
		{
			return false;
		}
	}

	void VoiceTuning::refresh(dsp56k::DSP& _dsp)
	{
		if(!m_installed || !m_mailbox.read(m_table, m_version)) return;
		for(unsigned note = 0; note < m_table.size(); ++note)
			_dsp.memWrite(dsp56k::MemArea_X, Table + note, static_cast<uint32_t>(m_table[note]) & 0xffffff);
	}
}
