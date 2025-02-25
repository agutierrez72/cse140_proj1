
#define MAXNUMINSTRS 1024	/* max # instrs in a program */
#define MAXNUMDATA 3072		/* max # data words */
/** 
 * Defining some constants to retrieve bits 
 **/
#define BITS_5  0x1f
#define BITS_6  0x3f
#define BITS_16 0xffff
#define BITS_26 0x3ffffff

struct SimulatedComputer {
    int memory [MAXNUMINSTRS+MAXNUMDATA];
    int registers [32];
    int pc;
    int printingRegisters, printingMemory, interactive, debugging;
};
typedef struct SimulatedComputer Computer;

typedef enum { R=0, I, J } InstrType;

// R-type constructs
typedef struct {
  int rs;
  int rt;
  int rd;
  int shamt;
  int funct;
} RRegs;

// I-type constructs
typedef struct {
  int rs;
  int rt;
  int addr_or_immed;
} IRegs;

// J-type constructs
typedef struct {
  int target;
} JRegs;

typedef struct {
  InstrType type;
  int op;
  union {
    RRegs r;
    IRegs i;
    JRegs j;
  } regs;
} DecodedInstr;

typedef struct {
  int R_rs; /*Value in register rs*/
  int R_rt;
  int R_rd;
} RegVals;

void InitComputer (FILE*, int printingRegisters, int printingMemory,
    int debugging, int interactive);
void Simulate ();
