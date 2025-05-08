#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include <vector>

using namespace llvm;

namespace {
class AVXLogicCombinerPass : public MachineFunctionPass {
  struct LogicPattern {
    unsigned FirstOp;
    unsigned SecondOp;
    unsigned CombinedOp;
    LogicPattern(unsigned F, unsigned S, unsigned C) 
            : FirstOp(F), SecondOp(S), CombinedOp(C) {}
  };
  DenseMap<unsigned, unsigned> OpcodeVariantMap;
  std::vector<LogicPattern> SupportedPatterns;
  const X86InstrInfo* TII;
  MachineRegisterInfo* MRI;

  void initMappings() {
    OpcodeVariantMap = {
      {X86::ANDPSrr, X86::VANDPSrr}, {X86::ORPSrr, X86::VORPSrr},
      {X86::XORPSrr, X86::VXORPSrr}, {X86::PANDrr,  X86::VPANDrr},
      {X86::PORrr,   X86::VPORrr},   {X86::PXORrr,  X86::VPXORrr}
    };

    SupportedPatterns.emplace_back(X86::PANDrr,  X86::PORrr,   X86::VPORrr);
    SupportedPatterns.emplace_back(X86::VPANDrr, X86::VPORrr,  X86::VPORrr);
    SupportedPatterns.emplace_back(X86::ANDPSrr, X86::ORPSrr,  X86::VORPSrr);
    SupportedPatterns.emplace_back(X86::PXORrr,  X86::PANDrr, X86::VPANDrr);
    SupportedPatterns.emplace_back(X86::VPXORrr, X86::VPANDrr,X86::VPANDrr);
    SupportedPatterns.emplace_back(X86::PANDNrr, X86::PORrr,   X86::VPORrr);
  }

  bool upgradeSingleInstruction(MachineInstr &MI) {
    auto It = OpcodeVariantMap.find(MI.getOpcode());
    if (It == OpcodeVariantMap.end()) {
      return false;
    }
    MachineBasicBlock &MBB = *MI.getParent();
    DebugLoc DL = MI.getDebugLoc();
    BuildMI(MBB, MI, DL, TII->get(It->second))
      .add(MI.getOperand(0))
      .add(MI.getOperand(1))
      .add(MI.getOperand(2));
    MI.eraseFromParent();
    return true;
  }

  bool combinePattern(MachineInstr &FirstMI, MachineInstr &SecondMI) {
    for (auto &Pattern : SupportedPatterns) {
      if (FirstMI.getOpcode() == Pattern.FirstOp && 
        SecondMI.getOpcode() == Pattern.SecondOp) {        
        MachineBasicBlock &MBB = *FirstMI.getParent();
        DebugLoc DL = FirstMI.getDebugLoc();

        Register TempReg = MRI->createVirtualRegister(
        MRI->getRegClass(FirstMI.getOperand(1).getReg()));

        BuildMI(MBB, SecondMI, DL, TII->get(Pattern.CombinedOp), TempReg)
          .addReg(FirstMI.getOperand(1).getReg())
          .addReg(FirstMI.getOperand(2).getReg());

        BuildMI(MBB, SecondMI, DL, TII->get(Pattern.CombinedOp),
                SecondMI.getOperand(0).getReg())
          .addReg(TempReg)
          .addReg(SecondMI.getOperand(2).getReg());

        FirstMI.eraseFromParent();
        SecondMI.eraseFromParent();
        return true;
      }
    }
    return false;
  }

public:
  static char ID;

  AVXLogicCombinerPass() : MachineFunctionPass(ID) {
    initMappings();
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    const X86Subtarget &ST = MF.getSubtarget<X86Subtarget>();
    if (!ST.hasAVX()) {
      return false;
    }

    TII = ST.getInstrInfo();
    MRI = &MF.getRegInfo();
    bool Changed = false;
    for (auto &MBB : MF) {
      auto MI = MBB.begin();
      while (MI != MBB.end()) {
        MachineInstr &CurrentMI = *MI++;
        if (upgradeSingleInstruction(CurrentMI)) {
          Changed = true;
          continue;
        }
        if (MI == MBB.end()) {
          continue;
        }
        MachineInstr &NextMI = *MI;
        if (MRI->hasOneUse(CurrentMI.getOperand(0).getReg()) &&
          combinePattern(CurrentMI, NextMI)) {
          Changed = true;
          MI++;
        }
      }
    }
    return Changed;
  }
};

char AVXLogicCombinerPass::ID = 0;
}
  
static RegisterPass<AVXLogicCombinerPass> 
  X("x86-logic-opt", "X86 Logical Operations Chain Optimizer", false,
    false);