y#include <stdio.h>
#include <stdlib.h>

#include "utils.h"

const int MAX_MEM_SIZE  = (1 << 13);

void fetchStage(int *icode, int *ifun, int *rA, int *rB, wordType *valC, wordType *valP) {
	wordType pcAddress = getPC();
	byteType instruction = getByteFromMemory(pcAddress);
	*icode = (instruction >> 4) & 0xf;
	*ifun = instruction & 0xf;
	if (*icode == IRMOVQ || *icode == RRMOVQ || *icode == OPQ || *icode == RMMOVQ || *icode == MRMOVQ ||*icode == PUSHQ || *icode == POPQ) {
		byteType registers = getByteFromMemory(pcAddress + 1);
		*rA = (registers >> 4) & 0xf;
		*rB = registers & 0xf;
	}
	if (*icode == IRMOVQ || *icode == RMMOVQ || *icode == MRMOVQ) {
		*valC = getWordFromMemory(pcAddress + 2);
		*valP = pcAddress + 10;
	}
	if (*icode == PUSHQ || *icode == POPQ || *icode == CMOVXX || *icode == OPQ || *icode == RRMOVQ) {
		*valP = pcAddress + 2;
	}
	if (*icode == NOP || *icode == RET || *icode == HALT) {
		*valP = pcAddress + 1;
	}
	if (*icode == HALT) {
		setStatus(STAT_HLT);
	}
	if (*icode == JXX || *icode == CALL) {
		*valC = getWordFromMemory(pcAddress + 1);
		*valP = pcAddress + 9;
	}
}

void decodeStage(int icode, int rA, int rB, wordType *valA, wordType *valB) {
	if (icode == HALT || icode == NOP) {
		return;
	}
	if (icode == CALL) {
		*valB = getRegister(RSP);
	}
	if (icode == RET || icode == POPQ) {
		*valB = getRegister(RSP);
		*valA = getRegister(RSP);
	}
	if (icode == PUSHQ) {
		*valA = getRegister(rA);
		*valB = getRegister(RSP);
	}
	if (icode == RMMOVQ  || icode == OPQ) {
		*valA = getRegister(rA);
		*valB = getRegister(rB);
	}
	if (icode == MRMOVQ) {
		*valB = getRegister(rB);
	}
	if (icode == RRMOVQ) {
		*valA = getRegister(rA);
	}
}

void executeStage(int icode, int ifun, wordType valA, wordType valB, wordType valC, wordType *valE, bool *Cnd) {
	if (icode == PUSHQ || icode == CALL ) {
		*valE = valB + (-8);
	}
	if (icode == RET || icode == POPQ) {
		*valE = valB + 8;
	}
	if (icode == RMMOVQ || icode == MRMOVQ) {
		*valE = valB + valA;
	}
	if (icode == JXX) {
		*Cnd = Cond(ifun);
	}
	if (icode == OPQ) {
    bool overflow = overflowFlag;
		//int ZF = 0;
		//int OF = 0;
		//int SF = 0;
		if (ifun == ADD) {
			*valE = valA + valB;
		}
		if (ifun == SUB) {
			*valE = valB - valA;
		}
		if (ifun == XOR) {
			if (valB == 1 && valA == 0) {
				*valE = 1;
			} else if (valB == 0 && valA == 1) {
				*valE = 1;
			} else {
				*valE = 0;
			}
		}
		
		
    	if (ifun == ADD || ifun == SUB) {
     	 	if (ifun == ADD) {
        	*valE = valB + valA;
      	} else {
        	*valE = valB - valA;
        	valA = -valA;
      	}
		overflow = (((valA < 0) && (valB < 0)) && (*valE >= 0)) || 
		(((valA > 0) && (valB > 0)) && (*valE < 0));
    	} else if (ifun == AND) {
      		*valE = valB & valA;
    	} else if (ifun == XOR) {
      	*valE = valB ^ valA;
   		}

    setFlags(*valE < 0, *valE == 0, overflow);
		/*
		if (*valE == 0) {
			ZF = 1;
		}
		//TODO: Check for overflow Flag
		if ((valA < 0) == (valB < 0) && ((valE < 0) != (valA < 0))) {
			OF = 1;
		}
		//Check for Sign flag
		if (*valE < 0) {
			SF = 1;
		}
		setFlags(SF, ZF, OF);*/
		//Help. valE = valB OP valA set CC
	}
	if (icode == RRMOVQ) {
		*valE = 0 + valA;
	}
	if (icode == IRMOVQ) {
		*valE = 0 + valC;
	}
	if (icode == MRMOVQ /*drb ra*/ || icode == RMMOVQ) {
		*valE = valB + valC;
	}
}

void memoryStage(int icode, wordType valA, wordType valP, wordType valE, wordType *valM) {
	if (icode == PUSHQ || icode == RMMOVQ) {
		setWordInMemory(valE, valA);
	}
	if (icode == POPQ) {
		setWordInMemory(*valM, valA);
	}
	if (icode == RET) {
		*valM = getWordFromMemory(valA);
	}
	if (icode == CALL) {
		setWordInMemory(valE, valP);
	}
	if (icode == MRMOVQ) {
		*valM = getWordFromMemory(valE);//setWordInMemory(*valM, valE);
	}
}

void writebackStage(int icode, wordType rA, wordType rB, wordType valE, wordType valM) {
	if (icode == MRMOVQ) {
		setRegister(rA, valM);
	}
	if (icode == OPQ || icode == RRMOVQ || icode == IRMOVQ) {
		setRegister(rB, valE);
	}
	if (icode == PUSHQ || icode == CALL || icode == RET) {
		setRegister(RSP, valE);
	}
	if (icode == POPQ) {
		setRegister(RSP, valE);
		setRegister(rA, valM);
	}
}

void pcUpdateStage(int icode, wordType valC, wordType valP, bool Cnd, wordType valM) {
	if (icode == PUSHQ || icode == POPQ || icode == RMMOVQ || icode == MRMOVQ || icode == OPQ || icode == RRMOVQ || icode == IRMOVQ) {
		setPC(valP);
	}
	if (icode == HALT) {
		setPC(valP);
		//setStatus(STAT_HLT);
	}
	if (icode == CALL) {
		setPC(valC);
	}
	if (icode == RET) {
		setPC(valM);
	}
	if (icode == JXX) { //HELP
	setPC(Cnd ? valC : valP);
	/*
		if (Cnd == TRUE){
			setPC(valC);
		} else {
			setPC(valP);
		}*/
	}
}

void stepMachine(int stepMode) {
  /* FETCH STAGE */
  int icode = 0, ifun = 0;
  int rA = 0, rB = 0;
  wordType valC = 0;
  wordType valP = 0;
 
  /* DECODE STAGE */
  wordType valA = 0;
  wordType valB = 0;

  /* EXECUTE STAGE */
  wordType valE = 0;
  bool Cnd = 0;

  /* MEMORY STAGE */
  wordType valM = 0;

  fetchStage(&icode, &ifun, &rA, &rB, &valC, &valP);
  applyStageStepMode(stepMode, "Fetch", icode, ifun, rA, rB, valC, valP, valA, valB, valE, Cnd, valM);

  decodeStage(icode, rA, rB, &valA, &valB);
  applyStageStepMode(stepMode, "Decode", icode, ifun, rA, rB, valC, valP, valA, valB, valE, Cnd, valM);
  
  executeStage(icode, ifun, valA, valB, valC, &valE, &Cnd);
  applyStageStepMode(stepMode, "Execute", icode, ifun, rA, rB, valC, valP, valA, valB, valE, Cnd, valM);
  
  memoryStage(icode, valA, valP, valE, &valM);
  applyStageStepMode(stepMode, "Memory", icode, ifun, rA, rB, valC, valP, valA, valB, valE, Cnd, valM);
  
  writebackStage(icode, rA, rB, valE, valM);
  applyStageStepMode(stepMode, "Writeback", icode, ifun, rA, rB, valC, valP, valA, valB, valE, Cnd, valM);
  
  pcUpdateStage(icode, valC, valP, Cnd, valM);
  applyStageStepMode(stepMode, "PC update", icode, ifun, rA, rB, valC, valP, valA, valB, valE, Cnd, valM);

  incrementCycleCounter();
}

/** 
 * main
 * */
int main(int argc, char **argv) {
  int stepMode = 0;
  FILE *input = parseCommandLine(argc, argv, &stepMode);

  initializeMemory(MAX_MEM_SIZE);
  initializeRegisters();
  loadMemory(input);

  applyStepMode(stepMode);
  while (getStatus() != STAT_HLT) {
    stepMachine(stepMode);
    applyStepMode(stepMode);
#ifdef DEBUG
    printMachineState();
    printf("\n");
#endif
  }
  printMachineState();
  return 0;
}