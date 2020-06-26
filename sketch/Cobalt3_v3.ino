/* Cobalt 3 OS v3.0
   This fancy opensource OS can manage the display and the keyboard of 'Cobalt 3' pocket computer, and runs a modified version of tiny basic
   Leonardo Leoni - www.el9000.cc - 18th June 2020
   Licensed under the MIT license: https://opensource.org/licenses/MIT
*/
#include <Keypad.h>
#include <SPI.h>

#define SERIAL_DISPLAY

// ASCII Characters
#define CR  '\r'
#define NL  '\n'
#define TAB '\t'
#define BELL  '\b'
#define DEL '\177'
#define SPACE   ' '
#define CTRLC 0x03
#define CTRLH 0x08
#define CTRLS 0x13
#define CTRLX 0x18

typedef short unsigned LINENUM;

// C Macros
#define car(x)             (((object *) (x))->car)
#define cdr(x)             (((object *) (x))->cdr)

const int RAMsize = RAMEND - RAMSTART + 1;
const int workspacesize = (RAMsize - RAMsize/4 - 280)/4;
const int buflen = 17;  // Length of longest symbol + 1

static uint8_t Cursor = 0;

enum function {  ENDFUNCTIONS };

// Typedefs

typedef struct sobject {
  union {
    struct {
      sobject *car;
      sobject *cdr;
    };
    struct {
       union {
        unsigned int name;
      };
    };
  };
} object;

typedef object *(*fn_ptr_type)(object *, object *);

typedef struct {
  const char *string;
} tbl_entry_t;

object workspace[workspacesize];

char buffer[buflen+1];

char *lookupstring (unsigned int name);
void Display (char c);

// ------------------------------------------------------
// keyboard config start
// ------------------------------------------------------
const byte ROWS = 8; //four rows
const byte COLS = 5; //four columns

char keys[ROWS][COLS] = {
  {'1','2','3','4','5'},
  {'q','w','e','r','t'},
  {'a','s','d','f','g'},
  {'@','z','x','c','v'},
  {'0','9','8','7','6'},
  {'p','o','i','u','y'},
  {'\r','l','k','j','h'},
  {' ','#','m','n','b'}
};

byte rowPins[ROWS] = {10, 11, 12, 14, 9, 4, 3, 2 }; //connect to the row pinouts of the keypad
byte colPins[COLS] = {15, 16, 17, 18, 19}; //connect to the column pinouts of the keypad

Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );
// ------------------------------------------------------
// keyboard config end
// ------------------------------------------------------

/*
 * modified tiny basic start
 ***********************************************************/
// Keyword table and constants - the last character has 0x80 added to it
static unsigned char keywords[] =
{
  'L', 'I', 'S', 'T' + 0x80,
  'L', 'O', 'A', 'D' + 0x80,
  'N', 'E', 'W' + 0x80,
  'R', 'U', 'N' + 0x80,
  'S', 'A', 'V', 'E' + 0x80,
  'N', 'E', 'X', 'T' + 0x80,
  'L', 'E', 'T' + 0x80,
  'I', 'F' + 0x80,
  'G', 'O', 'T', 'O' + 0x80,
  'G', 'O', 'S', 'U', 'B' + 0x80,
  'R', 'E', 'T', 'U', 'R', 'N' + 0x80,
  'R', 'E', 'M' + 0x80,
  'F', 'O', 'R' + 0x80,
  'I', 'N', 'P', 'U', 'T' + 0x80,
  'P', 'R', 'I', 'N', 'T' + 0x80,
  'P', 'O', 'K', 'E' + 0x80,
  'S', 'T', 'O', 'P' + 0x80,
  'B', 'Y', 'E' + 0x80,
  0
};

#define KW_LIST   0
#define KW_LOAD   1
#define KW_NEW    2
#define KW_RUN    3
#define KW_SAVE   4
#define KW_NEXT   5
#define KW_LET    6
#define KW_IF     7
#define KW_GOTO   8
#define KW_GOSUB  9
#define KW_RETURN 10
#define KW_REM    11
#define KW_FOR    12
#define KW_INPUT  13
#define KW_PRINT  14
#define KW_POKE   15
#define KW_STOP   16
#define KW_BYE    17
#define KW_DEFAULT  18

struct stack_for_frame
{
  char frame_type;
  char for_var;
  short int terminal;
  short int step;
  unsigned char *current_line;
  unsigned char *txtpos;
};

struct stack_gosub_frame
{
  char frame_type;
  unsigned char *current_line;
  unsigned char *txtpos;
};

static unsigned char func_tab[] =
{
  'P', 'E', 'E', 'K' + 0x80,
  'A', 'B', 'S' + 0x80,
  0
};
#define FUNC_PEEK    0
#define FUNC_ABS   1
#define FUNC_UNKNOWN 2

static unsigned char to_tab[] =
{
  'T', 'O' + 0x80,
  0
};

static unsigned char step_tab[] =
{
  'S', 'T', 'E', 'P' + 0x80,
  0
};

static unsigned char relop_tab[] =
{
  '>', '=' + 0x80,
  '<', '>' + 0x80,
  '>' + 0x80,
  '=' + 0x80,
  '<', '=' + 0x80,
  '<' + 0x80,
  0
};

#define RELOP_GE    0
#define RELOP_NE    1
#define RELOP_GT    2
#define RELOP_EQ    3
#define RELOP_LE    4
#define RELOP_LT    5
#define RELOP_UNKNOWN 6

#define VAR_SIZE sizeof(short int) // Size of variables in bytes

static unsigned char memory[1280];
static unsigned char *txtpos, *list_line;
static unsigned char expression_error;
static unsigned char *tempsp;
static unsigned char *stack_limit;
static unsigned char *program_start;
static unsigned char *program_end;
static unsigned char *variables_table;
static unsigned char *current_line;
static unsigned char *sp;
#define STACK_GOSUB_FLAG 'G'
#define STACK_FOR_FLAG 'F'
static unsigned char table_index;
static LINENUM linenum;

static const unsigned char okmsg[]    = "OK";
static const unsigned char badlinemsg[]   = "Invalid line number";
static const unsigned char invalidexprmsg[] = "Invalid expression";
static const unsigned char syntaxmsg[] = "Syntax Error";
static const unsigned char badinputmsg[] = "\nBad number";
static const unsigned char nomemmsg[] = "Not enough memory!";
static const unsigned char memorymsg[]  = " bytes free.";
static const unsigned char breakmsg[] = "break!";
static const unsigned char stackstuffedmsg[] = "Stack is stuffed!\n";
static const unsigned char unimplimentedmsg[] = "Unimplemented";
static const unsigned char backspacemsg[]   = "\b \b";

static int inchar(void);
static void outchar(unsigned char c);
static void line_terminator(void);
static short int expression(void);
static unsigned char breakcheck(void);

/***************************************************************************/
static void ignore_blanks(void)
{
  while(*txtpos == SPACE || *txtpos == TAB)
    txtpos++;
}

/***************************************************************************/
static void scantable(unsigned char *table)
{
  int i = 0;
  ignore_blanks();
  table_index = 0;
  while(1)
  {
    // Run out of table entries?
    if(table[0] == 0)
      return;

    // Do we match this character?
    if(txtpos[i] == table[0])
    {
      i++;
      table++;
    }
    else
    {
      // do we match the last character of keywork (with 0x80 added)? If so, return
      if(txtpos[i] + 0x80 == table[0])
      {
        txtpos += i + 1; // Advance the pointer to following the keyword
        ignore_blanks();
        return;
      }

      // Forward to the end of this keyword
      while((table[0] & 0x80) == 0)
        table++;

      // Now move on to the first character of the next word, and reset the position index
      table++;
      table_index++;
      i = 0;
    }
  }
}

/***************************************************************************/
static void pushb(unsigned char b)
{
  sp--;
  *sp = b;
}

/***************************************************************************/
static unsigned char popb()
{
  unsigned char b;
  b = *sp;
  sp++;
  return b;
}

/***************************************************************************/
static void printnum(int num)
{
  int digits = 0;

  if(num < 0)
  {
    num = -num;
    outchar('-');
  }

  do
  {
    pushb(num % 10 + '0');
    num = num / 10;
    digits++;
  }
  while (num > 0);

  while(digits > 0)
  {
    outchar(popb());
    digits--;
  }
}

/***************************************************************************/
static unsigned short testnum(void)
{
  unsigned short num = 0;
  ignore_blanks();

  while(*txtpos >= '0' && *txtpos <= '9' )
  {
    // Trap overflows
    if(num >= 0xFFFF / 10)
    {
      num = 0xFFFF;
      break;
    }

    num = num * 10 + *txtpos - '0';
    txtpos++;
  }
  return  num;
}

/***************************************************************************/
unsigned char check_statement_end(void)
{
  ignore_blanks();
  return (*txtpos == NL) || (*txtpos == ':');
}

/***************************************************************************/
static void printmsgNoNL(const unsigned char *msg) // ka
{
  while(*msg)
  {
    outchar(*msg);
    msg++;
  }
}

/***************************************************************************/
static unsigned char print_quoted_string(void)
{
  int i = 0;
  unsigned char delim = *txtpos;
  if(delim != '"' && delim != '\'')
    return 0;
  txtpos++;

  // Check we have a closing delimiter
  while(txtpos[i] != delim)
  {
    if(txtpos[i] == NL)
      return 0;
    i++;
  }

  // Print the characters
  while(*txtpos != delim)
  {
    outchar(*txtpos);
    txtpos++;
  }
  txtpos++; // Skip over the last delimiter
  ignore_blanks();

  return 1;
}

/***************************************************************************/
static void printmsg(const unsigned char *msg)
{
  printmsgNoNL(msg);
  line_terminator();
}

/***************************************************************************/
unsigned char getln(char prompt)
{
  //if (prompt != '>') // without basic language, OS cursor
    outchar(prompt);
  txtpos = program_end + sizeof(LINENUM);

  while(1)
  {
    char c = inchar();
    switch(c)
    {
      case CR:
      case NL:
        line_terminator();
        // Terminate all strings with a NL
        txtpos[0] = NL;
        return 1;
      case CTRLC:
        return 0;
      case CTRLH:
        if(txtpos == program_end)
          break;
        txtpos--;
        printmsgNoNL(backspacemsg);
        break;
      default:
        // We need to leave at least one space to allow us to shuffle the line into order
        if(txtpos == sp - 2)
          outchar(BELL);
        else
        {
          txtpos[0] = c;
          txtpos++;
          outchar(c);
        }
    }
  }
}

/***************************************************************************/
static unsigned char *findline(void)
{
  unsigned char *line = program_start;
  while(1)
  {
    if(line == program_end)
      return line;

    if(((LINENUM *)line)[0] >= linenum)
      return line;

    // Add the line length onto the current address, to get to the next line;
    line += line[sizeof(LINENUM)];
  }
}

/***************************************************************************/
static void toUppercaseBuffer(void)
{
  unsigned char *c = program_end + sizeof(LINENUM);
  unsigned char quote = 0;

  while(*c != NL)
  {
    // Are we in a quoted string?
    if(*c == quote)
      quote = 0;
    else if(*c == '"' || *c == '\'')
      quote = *c;
    else if(quote == 0 && *c >= 'a' && *c <= 'z')
      *c = *c + 'A' - 'a';
    c++;
  }
}

/***************************************************************************/
void printline()
{
  LINENUM line_num;

  line_num = *((LINENUM *)(list_line));
  list_line += sizeof(LINENUM) + sizeof(char);

  // Output the line */
  printnum(line_num);
  outchar(' ');
  while(*list_line != NL)
  {
    outchar(*list_line);
    list_line++;
  }
  list_line++;
  line_terminator();
}

/***************************************************************************/
static short int expr4(void)
{
  short int a = 0;

  if(*txtpos == '0')
  {
    txtpos++;
    a = 0;
    goto success;
  }

  if(*txtpos >= '1' && *txtpos <= '9')
  {
    do
    {
      a = a * 10 + *txtpos - '0';
      txtpos++;
    }
    while(*txtpos >= '0' && *txtpos <= '9');
    goto success;
  }

  // Is it a function or variable reference?
  if(txtpos[0] >= 'A' && txtpos[0] <= 'Z')
  {
    // Is it a variable reference (single alpha)
    if(txtpos[1] < 'A' || txtpos[1] > 'Z')
    {
      a = ((short int *)variables_table)[*txtpos - 'A'];
      txtpos++;
      goto success;
    }

    // Is it a function with a single parameter
    scantable(func_tab);
    if(table_index == FUNC_UNKNOWN)
      goto expr4_error;

    unsigned char f = table_index;

    if(*txtpos != '(')
      goto expr4_error;

    txtpos++;
    a = expression();
    if(*txtpos != ')')
      goto expr4_error;
    txtpos++;
    switch(f)
    {
      case FUNC_PEEK:
        a =  memory[a];
        goto success;
      case FUNC_ABS:
        if(a < 0)
          a = -a;
        goto success;
    }
  }

  if(*txtpos == '(')
  {
    txtpos++;
    a = expression();
    if(*txtpos != ')')
      goto expr4_error;

    txtpos++;
    goto success;
  }

expr4_error:
  expression_error = 1;
success:
  ignore_blanks();
  return a;
}

/***************************************************************************/
static short int expr3(void)
{
  short int a, b;

  a = expr4();
  while(1)
  {
    if(*txtpos == '*')
    {
      txtpos++;
      b = expr4();
      a *= b;
    }
    else if(*txtpos == '/')
    {
      txtpos++;
      b = expr4();
      if(b != 0)
        a /= b;
      else
        expression_error = 1;
    }
    else
      return a;
  }
}

/***************************************************************************/
static short int expr2(void)
{
  short int a, b;

  if(*txtpos == '-' || *txtpos == '+')
    a = 0;
  else
    a = expr3();

  while(1)
  {
    if(*txtpos == '-')
    {
      txtpos++;
      b = expr3();
      a -= b;
    }
    else if(*txtpos == '+')
    {
      txtpos++;
      b = expr3();
      a += b;
    }
    else
      return a;
  }
}
/***************************************************************************/
static short int expression(void)
{
  short int a, b;

  a = expr2();
  // Check if we have an error
  if(expression_error)  return a;

  scantable(relop_tab);
  if(table_index == RELOP_UNKNOWN)
    return a;

  switch(table_index)
  {
    case RELOP_GE:
      b = expr2();
      if(a >= b) return 1;
      break;
    case RELOP_NE:
      b = expr2();
      if(a != b) return 1;
      break;
    case RELOP_GT:
      b = expr2();
      if(a > b) return 1;
      break;
    case RELOP_EQ:
      b = expr2();
      if(a == b) return 1;
      break;
    case RELOP_LE:
      b = expr2();
      if(a <= b) return 1;
      break;
    case RELOP_LT:
      b = expr2();
      if(a < b) return 1;
      break;
  }
  return 0;
}

/***************************************************************************/

// Read/Evaluate/Print loop
void loop()
{
  unsigned char *start;
  unsigned char *newEnd;
  unsigned char linelen;

/*  other tiny basic modification start
 *   
 */
  
  variables_table = memory;
  program_start = memory + 27 * VAR_SIZE;
  program_end = program_start;
  sp = memory + sizeof(memory); // Needed for printnum
  printnum(sp - program_end);

  Cursor = 1;
  printmsg(memorymsg);
  
warmstart:     
  // this signifies that it is running in 'direct' mode.
  current_line = 0;
  sp = memory + sizeof(memory);
  printmsg(okmsg);

prompt:

  while(!getln('>'))
    line_terminator();

  toUppercaseBuffer();
  txtpos = program_end + sizeof(unsigned short);

  // Find the end of the freshly entered line
  while(*txtpos != NL)
    txtpos++;

  // Move it to the end of program_memory
  {
    unsigned char *dest;
    dest = sp - 1;
    while(1)
    {
      *dest = *txtpos;
      if(txtpos == program_end + sizeof(unsigned short))
        break;
      dest--;
      txtpos--;
    }
    txtpos = dest;
  }

  // Now see if we have a line number
  linenum = testnum();
  ignore_blanks();
  if(linenum == 0)
    goto direct;

  if(linenum == 0xFFFF)
    goto badline;

  // Find the length of what is left, including the (yet-to-be-populated) line header
  linelen = 0;
  while(txtpos[linelen] != NL)
    linelen++;
  linelen++; // Include the NL in the line length
  linelen += sizeof(unsigned short) + sizeof(char); // Add space for the line number and line length

  // Now we have the number, add the line header.
  txtpos -= 3;
  *((unsigned short *)txtpos) = linenum;
  txtpos[sizeof(LINENUM)] = linelen;

  // Merge it into the rest of the program
  start = findline();

  // If a line with that number exists, then remove it
  if(start != program_end && *((LINENUM *)start) == linenum)
  {
    unsigned char *dest, *from;
    unsigned tomove;

    from = start + start[sizeof(LINENUM)];
    dest = start;

    tomove = program_end - from;
    while( tomove > 0)
    {
      *dest = *from;
      from++;
      dest++;
      tomove--;
    }
    program_end = dest;
  }

  if(txtpos[sizeof(LINENUM) + sizeof(char)] == NL) // If the line has no txt, it was just a delete
    goto prompt;

  // Make room for the new line, either all in one hit or lots of little shuffles
  while(linelen > 0)
  {
    unsigned int tomove;
    unsigned char *from, *dest;
    unsigned int space_to_make;

    space_to_make = txtpos - program_end;

    if(space_to_make > linelen)
      space_to_make = linelen;
    newEnd = program_end + space_to_make;
    tomove = program_end - start;

    // Source and destination - as these areas may overlap we need to move bottom up
    from = program_end;
    dest = newEnd;
    while(tomove > 0)
    {
      from--;
      dest--;
      *dest = *from;
      tomove--;
    }

    // Copy over the bytes into the new space
    for(tomove = 0; tomove < space_to_make; tomove++)
    {
      *start = *txtpos;
      txtpos++;
      start++;
      linelen--;
    }
    program_end = newEnd;
  }
  goto prompt;

unimplemented:
  printmsg(unimplimentedmsg);
  goto prompt;

badline:
  printmsg(badlinemsg);
  goto prompt;
invalidexpr:
  printmsg(invalidexprmsg);
  goto prompt;
syntaxerror:
  //Cursor = 1; delete the cursor before syntx error or ...
  printmsg(syntaxmsg); // ok
  if(current_line != (void *)0)
  {
    unsigned char tmp = *txtpos;
    if(*txtpos != NL)
      *txtpos = '^';
    list_line = current_line;
    printline();
    *txtpos = tmp;
  }
  line_terminator();
  goto prompt;

stackstuffed:
  printmsg(stackstuffedmsg);
  goto warmstart;
nomem:
  printmsg(nomemmsg);
  goto warmstart;

run_next_statement:
  while(*txtpos == ':')
    txtpos++;
  ignore_blanks();
  if(*txtpos == NL)
    goto execnextline;
  goto interperateAtTxtpos;

direct:
  txtpos = program_end + sizeof(LINENUM);
  if(*txtpos == NL)
    goto prompt;

interperateAtTxtpos:
  if(breakcheck())
  {
    printmsg(breakmsg);
    goto warmstart;
  }

  scantable(keywords);
  ignore_blanks();

  switch(table_index)
  {
    case KW_LIST:
      goto list;
    case KW_LOAD:
      goto unimplemented; 
    case KW_NEW:
      if(txtpos[0] != NL)
        goto syntaxerror;
      program_end = program_start;
      goto prompt;
    case KW_RUN:
      current_line = program_start;
      goto execline;
    case KW_SAVE:
      goto unimplemented; 
    case KW_NEXT:
      goto next;
    case KW_LET:
      goto assignment;
    case KW_IF:
    {
      short int val;
      expression_error = 0;
      val = expression();
      if(expression_error || *txtpos == NL)
        goto invalidexpr;
      if(val != 0)
        goto interperateAtTxtpos;
      goto execnextline;
    }
    case KW_GOTO:
      expression_error = 0;
      linenum = expression();
      if(expression_error || *txtpos != NL)
        goto invalidexpr;
      current_line = findline();
      goto execline;

    case KW_GOSUB:
      goto gosub;
    case KW_RETURN:
      goto gosub_return;
    case KW_REM:
      goto execnextline;  // Ignore line completely
    case KW_FOR:
      goto forloop;
    case KW_INPUT:
      goto input;
    case KW_PRINT:
      goto print;
    case KW_POKE:
      goto poke;
    case KW_STOP:
      // This is the easy way to end - set the current line to the end of program attempt to run it
      if(txtpos[0] != NL)
        goto syntaxerror;
      current_line = program_end;
      goto execline;
    case KW_BYE:
      // Leave the basic interperater
      return;
    case KW_DEFAULT:
      goto assignment;
    default:
      break;
  }

execnextline:
  if(current_line == (void *)0)   // Processing direct commands?
    goto prompt;
  current_line +=  current_line[sizeof(LINENUM)];

execline:
  if(current_line == program_end) // Out of lines to run
    goto warmstart;
  txtpos = current_line + sizeof(LINENUM) + sizeof(char);
  goto interperateAtTxtpos;

input:
  {
    unsigned char isneg = 0;
    short int *var;
    ignore_blanks();
    if(*txtpos < 'A' || *txtpos > 'Z')
      goto syntaxerror;
    var = ((short int *)variables_table) + *txtpos - 'A';
    txtpos++;
    if(!check_statement_end())
      goto syntaxerror;
again:
    if(!getln('?'))
      goto warmstart;

    // Go to where the buffer is read
    txtpos = program_end + sizeof(LINENUM);
    if(*txtpos == '-')
    {
      isneg = 1;
      txtpos++;
    }

    *var = 0;
    do
    {
      *var = *var * 10 + *txtpos - '0';
      txtpos++;
    }
    while(*txtpos >= '0' && *txtpos <= '9');
    ignore_blanks();
    if(*txtpos != NL)
    {
      printmsg(badinputmsg);
      goto again;
    }

    if(isneg)
      *var = -*var;

    goto run_next_statement;
  }
forloop:
  {
    unsigned char var;
    short int initial, step, terminal;

    if(*txtpos < 'A' || *txtpos > 'Z')
      goto syntaxerror;
    var = *txtpos;
    txtpos++;

    scantable(relop_tab);
    if(table_index != RELOP_EQ)
      goto syntaxerror;

    expression_error = 0;
    initial = expression();
    if(expression_error)
      goto invalidexpr;

    scantable(to_tab);
    if(table_index != 0)
      goto syntaxerror;

    terminal = expression();
    if(expression_error)
      goto invalidexpr;

    scantable(step_tab);
    if(table_index == 0)
    {
      step = expression();
      if(expression_error)
        goto invalidexpr;
    }
    else
      step = 1;
    if(!check_statement_end())
      goto syntaxerror;

    if(!expression_error && *txtpos == NL)
    {
      struct stack_for_frame *f;
      if(sp + sizeof(struct stack_for_frame) < stack_limit)
        goto nomem;

      sp -= sizeof(struct stack_for_frame);
      f = (struct stack_for_frame *)sp;
      ((short int *)variables_table)[var - 'A'] = initial;
      f->frame_type = STACK_FOR_FLAG;
      f->for_var = var;
      f->terminal = terminal;
      f->step     = step;
      f->txtpos   = txtpos;
      f->current_line = current_line;
      goto run_next_statement;
    }
  }
  goto syntaxerror;

gosub:
  expression_error = 0;
  linenum = expression();
  if(expression_error)
    goto invalidexpr;
  if(!expression_error && *txtpos == NL)
  {
    struct stack_gosub_frame *f;
    if(sp + sizeof(struct stack_gosub_frame) < stack_limit)
      goto nomem;

    sp -= sizeof(struct stack_gosub_frame);
    f = (struct stack_gosub_frame *)sp;
    f->frame_type = STACK_GOSUB_FLAG;
    f->txtpos = txtpos;
    f->current_line = current_line;
    current_line = findline();
    goto execline;
  }
  goto syntaxerror;

next:
  // Fnd the variable name
  ignore_blanks();
  if(*txtpos < 'A' || *txtpos > 'Z')
    goto syntaxerror;
  txtpos++;
  if(!check_statement_end())
    goto syntaxerror;

gosub_return:
  // Now walk up the stack frames and find the frame we want, if present
  tempsp = sp;
  while(tempsp < memory + sizeof(memory) - 1)
  {
    switch(tempsp[0])
    {
      case STACK_GOSUB_FLAG:
        if(table_index == KW_RETURN)
        {
          struct stack_gosub_frame *f = (struct stack_gosub_frame *)tempsp;
          current_line  = f->current_line;
          txtpos      = f->txtpos;
          sp += sizeof(struct stack_gosub_frame);
          goto run_next_statement;
        }
        // This is not the loop you are looking for... so Walk back up the stack
        tempsp += sizeof(struct stack_gosub_frame);
        break;
      case STACK_FOR_FLAG:
        // Flag, Var, Final, Step
        if(table_index == KW_NEXT)
        {
          struct stack_for_frame *f = (struct stack_for_frame *)tempsp;
          // Is the the variable we are looking for?
          if(txtpos[-1] == f->for_var)
          {
            short int *varaddr = ((short int *)variables_table) + txtpos[-1] - 'A';
            *varaddr = *varaddr + f->step;
            // Use a different test depending on the sign of the step increment
            if((f->step > 0 && *varaddr <= f->terminal) || (f->step < 0 && *varaddr >= f->terminal))
            {
              // We have to loop so don't pop the stack
              txtpos = f->txtpos;
              current_line = f->current_line;
              goto run_next_statement;
            }
            // We've run to the end of the loop. drop out of the loop, popping the stack
            sp = tempsp + sizeof(struct stack_for_frame);
            goto run_next_statement;
          }
        }
        // This is not the loop you are looking for... so Walk back up the stack
        tempsp += sizeof(struct stack_for_frame);
        break;
      default:
        goto stackstuffed;
    }
  }
  // Didn't find the variable we've been looking for
  goto syntaxerror;

assignment:
  {
    short int value;
    short int *var;

    if(*txtpos < 'A' || *txtpos > 'Z')
      goto syntaxerror;
    var = (short int *)variables_table + *txtpos - 'A';
    txtpos++;

    ignore_blanks();

    if (*txtpos != '=')
      goto syntaxerror;
    txtpos++;
    ignore_blanks();
    expression_error = 0;
    value = expression();
    if(expression_error)
      goto invalidexpr;
    // Check that we are at the end of the statement
    if(!check_statement_end())
      goto syntaxerror;
    *var = value;
  }
  goto run_next_statement;
poke:
  {
    // Work out where to put it
    expression_error = 0;
    if(expression_error)
      goto invalidexpr;

    // check for a comma
    ignore_blanks();
    if (*txtpos != ',')
      goto syntaxerror;
    txtpos++;
    ignore_blanks();

    // Now get the value to assign
    expression_error = 0;
    if(expression_error)
      goto invalidexpr;

    // Check that we are at the end of the statement
    if(!check_statement_end())
      goto syntaxerror;
  }
  goto run_next_statement;

list:
  linenum = testnum(); // Retuns 0 if no line found.

  // Should be EOL
  if(txtpos[0] != NL)
    goto syntaxerror;

  // Find the line
  list_line = findline();
  while(list_line != program_end)
    printline();
  goto warmstart;

print:
  // If we have an empty list then just put out a NL
  if(*txtpos == ':' )
  {
    line_terminator();
    txtpos++;
    goto run_next_statement;
  }
  if(*txtpos == NL)
  {
    goto execnextline;
  }

  while(1)
  {
    ignore_blanks();
    if(print_quoted_string())
    {
      ;
    }
    else if(*txtpos == '"' || *txtpos == '\'')
      goto syntaxerror;
    else
    {
      short int e;
      expression_error = 0;
      e = expression();
      if(expression_error)
        goto invalidexpr;
      printnum(e);
    }

    // At this point we have three options, a comma or a new line
    if(*txtpos == ',')
      txtpos++; // Skip the comma and move onto the next
    else if(txtpos[0] == ';' && (txtpos[1] == NL || txtpos[1] == ':'))
    {
      txtpos++; // This has to be the end of the print - no newline
      break;
    }
    else if(check_statement_end())
    {
      line_terminator();  // The end of the print statement
      break;
    }
    else
      goto syntaxerror;
  }
  goto run_next_statement;
}

/***************************************************************************/
static void line_terminator(void)
{
  outchar(NL);
  outchar(CR);
}

/***********************************************************/
static unsigned char breakcheck(void)
{
  if(Serial.available())
    return Serial.read() == CTRLC;
  return 0;
}
/***********************************************************/
String msg;   
const char Kmaplev1[]  = "12345qwertasdfg@zxcv09876poiuy\rlkjh #mnb";
const char Kmaplev2[]  = "!@#$%<>   \\|    :'?/\*)(&^\b\";_  =+-   .,*";
unsigned  char keyle;
static int inchar()
{
  while(1)
  {
    if (keypad.getKeys())
    {
      for (int i=0; i<LIST_MAX; i++)   // Scan the whole key list.
      {
        if ( keypad.key[i].stateChanged )   // Only find keys that have changed state.
        {
          switch (keypad.key[0].kstate) {  // Report active key state : IDLE, PRESSED, HOLD, or RELEASED
            case PRESSED:
              msg = " PRESSED.";
              keyle  = keypad.key[0].kcode;
              if (keypad.key[1].kstate == NULL && keypad.key[0].kcode != 15){
                return keypad.key[0].kchar;
              }
              break;
            case HOLD:
              if (keypad.key[0].kcode == 15 && keypad.key[1].kstate == PRESSED){
                return Kmaplev2[keypad.key[1].kcode];
              }
              msg = " HOLD.";
              break;
            case RELEASED:
              msg = " RELEASED.";
              break;
            case IDLE:
              msg = " IDLE.";
          }
        }
      }
    }
    //if(Serial.available())
      //return Serial.read();  // with this comment it doesn't read from serial monitor no more
  }
}

/***********************************************************/
static void outchar(unsigned char c)
{
#ifdef SERIAL_DISPLAY
  Display(c);
#else
  Serial.write(c);
#endif
}

/*    tiny basic modification end   
*/
// Set up workspace
void initworkspace () {
  for (int i=workspacesize-1; i>=0; i--) {
    object *obj = &workspace[i];
    car(obj) = NULL;
  }
}

// Save-image and load-image
typedef struct {
  unsigned int eval;
  unsigned int datasize;
  char data[];
} struct_image;

struct_image EEMEM image;

// Helper functions

char *name(object *obj){
  buffer[3] = '\0';
  unsigned int x = obj->name;
  if (x < ENDFUNCTIONS) return lookupstring(x);
  for (int n=2; n>=0; n--) {
    x = x / 40;
  }
  return buffer;
}

// Table lookup functions
char *lookupstring () {
  return buffer;
}

// Print functions
void pchar (char c) {
  Display(c);
}

void pfstring (const __FlashStringHelper *s) {
  PGM_P p = reinterpret_cast<PGM_P>(s);
  while (1) {
    char c = pgm_read_byte(p++);
    if (c == 0) return;
    pchar(c);
  }
}

void pln () {
  pchar('\r'); pchar('\n');
}

// Terminal
int const SH1106 = 1;     // Set to 0 for SSD1306 or 1 for SH1106

int const datapin = 8;    
int const data = 0;       // Bit in PORTB

// These are the same as their positions in PORTD
int const clk = 7;
int const dc = 6;
int const cs = 5;

// Character set - stored in program memory
const uint8_t CharMap[96][6] PROGMEM = {
{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 
{ 0x00, 0x00, 0x5F, 0x00, 0x00, 0x00 }, 
{ 0x00, 0x07, 0x00, 0x07, 0x00, 0x00 }, 
{ 0x14, 0x7F, 0x14, 0x7F, 0x14, 0x00 }, 
{ 0x24, 0x2A, 0x7F, 0x2A, 0x12, 0x00 }, 
{ 0x23, 0x13, 0x08, 0x64, 0x62, 0x00 }, 
{ 0x36, 0x49, 0x56, 0x20, 0x50, 0x00 }, 
{ 0x00, 0x08, 0x07, 0x03, 0x00, 0x00 }, 
{ 0x00, 0x1C, 0x22, 0x41, 0x00, 0x00 }, 
{ 0x00, 0x41, 0x22, 0x1C, 0x00, 0x00 }, 
{ 0x2A, 0x1C, 0x7F, 0x1C, 0x2A, 0x00 }, 
{ 0x08, 0x08, 0x3E, 0x08, 0x08, 0x00 }, 
{ 0x00, 0x80, 0x70, 0x30, 0x00, 0x00 }, 
{ 0x08, 0x08, 0x08, 0x08, 0x08, 0x00 }, 
{ 0x00, 0x00, 0x60, 0x60, 0x00, 0x00 }, 
{ 0x20, 0x10, 0x08, 0x04, 0x02, 0x00 }, 
{ 0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00 }, 
{ 0x00, 0x42, 0x7F, 0x40, 0x00, 0x00 }, 
{ 0x72, 0x49, 0x49, 0x49, 0x46, 0x00 }, 
{ 0x21, 0x41, 0x49, 0x4D, 0x33, 0x00 }, 
{ 0x18, 0x14, 0x12, 0x7F, 0x10, 0x00 }, 
{ 0x27, 0x45, 0x45, 0x45, 0x39, 0x00 }, 
{ 0x3C, 0x4A, 0x49, 0x49, 0x31, 0x00 }, 
{ 0x41, 0x21, 0x11, 0x09, 0x07, 0x00 }, 
{ 0x36, 0x49, 0x49, 0x49, 0x36, 0x00 }, 
{ 0x46, 0x49, 0x49, 0x29, 0x1E, 0x00 }, 
{ 0x00, 0x00, 0x14, 0x00, 0x00, 0x00 }, 
{ 0x00, 0x40, 0x34, 0x00, 0x00, 0x00 }, 
{ 0x00, 0x08, 0x14, 0x22, 0x41, 0x00 }, 
{ 0x14, 0x14, 0x14, 0x14, 0x14, 0x00 }, 
{ 0x00, 0x41, 0x22, 0x14, 0x08, 0x00 }, 
{ 0x02, 0x01, 0x59, 0x09, 0x06, 0x00 }, 
{ 0x3E, 0x41, 0x5D, 0x59, 0x4E, 0x00 }, 
{ 0x7C, 0x12, 0x11, 0x12, 0x7C, 0x00 }, 
{ 0x7F, 0x49, 0x49, 0x49, 0x36, 0x00 }, 
{ 0x3E, 0x41, 0x41, 0x41, 0x22, 0x00 }, 
{ 0x7F, 0x41, 0x41, 0x41, 0x3E, 0x00 }, 
{ 0x7F, 0x49, 0x49, 0x49, 0x41, 0x00 }, 
{ 0x7F, 0x09, 0x09, 0x09, 0x01, 0x00 }, 
{ 0x3E, 0x41, 0x41, 0x51, 0x73, 0x00 }, 
{ 0x7F, 0x08, 0x08, 0x08, 0x7F, 0x00 }, 
{ 0x00, 0x41, 0x7F, 0x41, 0x00, 0x00 }, 
{ 0x20, 0x40, 0x41, 0x3F, 0x01, 0x00 }, 
{ 0x7F, 0x08, 0x14, 0x22, 0x41, 0x00 }, 
{ 0x7F, 0x40, 0x40, 0x40, 0x40, 0x00 }, 
{ 0x7F, 0x02, 0x1C, 0x02, 0x7F, 0x00 }, 
{ 0x7F, 0x04, 0x08, 0x10, 0x7F, 0x00 }, 
{ 0x3E, 0x41, 0x41, 0x41, 0x3E, 0x00 }, 
{ 0x7F, 0x09, 0x09, 0x09, 0x06, 0x00 }, 
{ 0x3E, 0x41, 0x51, 0x21, 0x5E, 0x00 }, 
{ 0x7F, 0x09, 0x19, 0x29, 0x46, 0x00 }, 
{ 0x26, 0x49, 0x49, 0x49, 0x32, 0x00 }, 
{ 0x03, 0x01, 0x7F, 0x01, 0x03, 0x00 }, 
{ 0x3F, 0x40, 0x40, 0x40, 0x3F, 0x00 }, 
{ 0x1F, 0x20, 0x40, 0x20, 0x1F, 0x00 }, 
{ 0x3F, 0x40, 0x38, 0x40, 0x3F, 0x00 }, 
{ 0x63, 0x14, 0x08, 0x14, 0x63, 0x00 }, 
{ 0x03, 0x04, 0x78, 0x04, 0x03, 0x00 }, 
{ 0x61, 0x59, 0x49, 0x4D, 0x43, 0x00 }, 
{ 0x00, 0x7F, 0x41, 0x41, 0x41, 0x00 }, 
{ 0x02, 0x04, 0x08, 0x10, 0x20, 0x00 }, 
{ 0x00, 0x41, 0x41, 0x41, 0x7F, 0x00 }, 
{ 0x04, 0x02, 0x01, 0x02, 0x04, 0x00 }, 
{ 0x40, 0x40, 0x40, 0x40, 0x40, 0x00 }, 
{ 0x00, 0x03, 0x07, 0x08, 0x00, 0x00 }, 
{ 0x20, 0x54, 0x54, 0x78, 0x40, 0x00 }, 
{ 0x7F, 0x28, 0x44, 0x44, 0x38, 0x00 }, 
{ 0x38, 0x44, 0x44, 0x44, 0x28, 0x00 }, 
{ 0x38, 0x44, 0x44, 0x28, 0x7F, 0x00 }, 
{ 0x38, 0x54, 0x54, 0x54, 0x18, 0x00 }, 
{ 0x00, 0x08, 0x7E, 0x09, 0x02, 0x00 }, 
{ 0x18, 0xA4, 0xA4, 0x9C, 0x78, 0x00 }, 
{ 0x7F, 0x08, 0x04, 0x04, 0x78, 0x00 }, 
{ 0x00, 0x44, 0x7D, 0x40, 0x00, 0x00 }, 
{ 0x20, 0x40, 0x40, 0x3D, 0x00, 0x00 }, 
{ 0x7F, 0x10, 0x28, 0x44, 0x00, 0x00 }, 
{ 0x00, 0x41, 0x7F, 0x40, 0x00, 0x00 }, 
{ 0x7C, 0x04, 0x78, 0x04, 0x78, 0x00 }, 
{ 0x7C, 0x08, 0x04, 0x04, 0x78, 0x00 }, 
{ 0x38, 0x44, 0x44, 0x44, 0x38, 0x00 }, 
{ 0xFC, 0x18, 0x24, 0x24, 0x18, 0x00 }, 
{ 0x18, 0x24, 0x24, 0x18, 0xFC, 0x00 }, 
{ 0x7C, 0x08, 0x04, 0x04, 0x08, 0x00 }, 
{ 0x48, 0x54, 0x54, 0x54, 0x24, 0x00 }, 
{ 0x04, 0x04, 0x3F, 0x44, 0x24, 0x00 }, 
{ 0x3C, 0x40, 0x40, 0x20, 0x7C, 0x00 }, 
{ 0x1C, 0x20, 0x40, 0x20, 0x1C, 0x00 }, 
{ 0x3C, 0x40, 0x30, 0x40, 0x3C, 0x00 }, 
{ 0x44, 0x28, 0x10, 0x28, 0x44, 0x00 }, 
{ 0x4C, 0x90, 0x90, 0x90, 0x7C, 0x00 }, 
{ 0x44, 0x64, 0x54, 0x4C, 0x44, 0x00 }, 
{ 0x00, 0x08, 0x36, 0x41, 0x00, 0x00 }, 
{ 0x00, 0x00, 0x77, 0x00, 0x00, 0x00 }, 
{ 0x00, 0x41, 0x36, 0x08, 0x00, 0x00 }, 
{ 0x02, 0x01, 0x02, 0x04, 0x02, 0x00 }, 
{ 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0 }
};

// initialization sequence for OLED module
int const InitLen = 23;
unsigned char Init[InitLen] = {
  0xAE, // Display off
  0xD5, // Set display clock
  0x80, // Recommended value
  0xA8, // Set multiplex
  0x3F,
  0xD3, // Set display offset
  0x00,
  0x40, // Zero start line
  0x8D, // Charge pump
  0x14,
  0x20, // Memory mode
  0x02, // Page addressing
  0xA1, // 0xA0/0xA1 flip horizontally
  0xC8, // 0xC0/0xC8 flip vertically
  0xDA, // Set comp ins
  0x12,
  0x81, // Set contrast
  0x7F,
  0xD9, // Set pre charge
  0xF1,
  0xDB, // Set vcom detect
  0x40,
  0xA6  // Normal (0xA7=Inverse)
};

// Write a data byte to the display
void Data (uint8_t d) {  
  PIND = 1<<cs; // cs low
  for (uint8_t bit = 0x80; bit; bit >>= 1) {
    PIND = 1<<clk; // clk low
    if (d & bit) PORTB = PORTB | (1<<data); else PORTB = PORTB & ~(1<<data);
    PIND = 1<<clk; // clk high
  }
  PIND = 1<<cs; // cs high
}

// Write a command byte to the display
void Command (uint8_t c) { 
  PIND = 1<<dc; // dc low
  Data(c);
  PIND = 1<<dc; // dc high
}

void InitDisplay () {
  // Define pins
  pinMode(dc, OUTPUT); digitalWrite(dc,HIGH);
  pinMode(clk, OUTPUT); digitalWrite(clk,HIGH);
  pinMode(datapin, OUTPUT);
  pinMode(cs, OUTPUT); digitalWrite(cs,HIGH);
  for (uint8_t c=0; c<InitLen; c++) Command(Init[c]);
  Display(12);    // Clear display
  Command(0xAF);  // Display on
}

// Character terminal **********************************************

void ClearLine (uint8_t line) {
  Command(0xB0 + line);
  Command(0x00); // Column start low
  Command(0x00); // Column start high
  for (uint8_t b = 0 ; b < 128 + 4*SH1106; b++) Data(0);
}

// Clears the top line, then scrolls the display up by one line
void ScrollDisplay (uint8_t *scroll) {
  ClearLine(*scroll);
  *scroll = (*scroll + 1) & 0x07;
  Command(0xD3);
  Command(*scroll << 3);
}

// Plots a character; line = 0 to 7; column = 0 to 20
void PlotChar (char c, uint8_t line, uint8_t column) {
  column = column*6+2*SH1106;
  Command(0xB0 + (line & 0x07));
  Command(0x00 + (column & 0x0F)); // Column start low
  Command(0x10 + (column >> 4));   // Column start high
  for (uint8_t col = 0; col < 6; col++) {
    Data(pgm_read_byte(&CharMap[(c & 0x7F)-32][col]));
  }
}
  
// Prints a character to display, with cursor, handling control characters
void Display (char c) { 
  static uint8_t Line = 0, Column = 0, Scroll = 0;
  // Hide cursor
  PlotChar(' ', Line+Scroll, Column); // ok
  if (c >= 32) {                  // Normal character
    PlotChar(c, Line+Scroll, Column++);
    if (Column > 20) {
      Column = 0;
      if (Line == 7) ScrollDisplay(&Scroll); else Line++;
    }
  // Control characters
  } else if (c == 8) {             // Backspace
    if (Column == 0) {
      Line--;
      Column = 20;
    } else Column--;
  } else if (c == 9) {             // Cursor forward
    if (Column == 20) {
      Line++;
      Column = 0;
    } else Column++;
  } else if (c == 12) {            // Clear display
    for (uint8_t p=0; p < 8; p++) ClearLine(p);
    Line = 0;
    Column = 0;
  } else if (c == 13) {            // Return
    Column = 0;
    if (Line == 7) ScrollDisplay(&Scroll); else Line++;
    if (Cursor == 0) {
      //pfstring(F("> "));  // without basic language, OS cursor
    } else {
      Cursor = 0;
    }
  }
  // Show cursor
  PlotChar(0x7F, Line+Scroll, Column);
}

// Keyboard
const int DataPin = 4;

// Setup
void setup() {
  // important!
  InitDisplay();
  Serial.begin(9600);

  pfstring(F("Cobalt3 OS. Basic v1"));
  Cursor = 1;
  pln();
}
