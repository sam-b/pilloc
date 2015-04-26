#include "pin.H"
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <set>
#include <string>
#include <algorithm>
#include <stdexcept>

/* ===================================================================== */
/* Names of malloc and free */
/* ===================================================================== */
#define CALLOC "calloc"
#define MALLOC "malloc"
#define FREE "free"
#define REALLOC "realloc"

using namespace std;

/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */

class State;
class Block;
class Empty;

void generate_html(void);

string ADDRINTToString (ADDRINT a)
{
    ostringstream temp;
    temp << "0x" << hex <<a;
    return temp.str();
}

string IntToString (int a)
{
    ostringstream temp;
    temp<<a;
    return temp.str();
}

int RandU(int nMin, int nMax)
{
    return nMin + (int)((double)rand() / (RAND_MAX) * (nMax-nMin+1));
}

std::ofstream TraceFile;

int unit_width = 10;
set<ADDRINT> boundaries;
vector<State> timeline;

class Block
{
public:
    Block();
    ~Block();
    ADDRINT start();
    ADDRINT end();
    string gen_html(int);
    string color;
    string get_color();
    int header;
    int footer;
    int round;
    int minsz;
    bool error;
    ADDRINT addr;
    ADDRINT size;
private:
    int red;
    int green;
    int blue;
};

Block::Block()
{
    this->header = 8;
    this->footer = 0;
    this->round = 0x10;
    this->minsz = 0x20;
    this->error = false;
    this->color = this->get_color();
}

Block::~Block()
{

}

ADDRINT Block::start()
{
    return this->addr - this->header;
}

ADDRINT Block::end()
{
    ADDRINT size = max((ADDRINT) this->minsz, this->size + this->header + this->footer);
    ADDRINT rsize = size + (this->round - 1);
    rsize = rsize - (rsize % this->round);
    return this->addr - this->header + rsize;
}

string Block::get_color()
{
    string color = "background-color: rgb(";
    color += IntToString(RandU(0,255));
    color+=", ";
    color+= IntToString(RandU(0,255));
    color+=", ";
    color += IntToString(RandU(0,255));
    color+=");";
    return color;
}

string Block::gen_html(int width)
{
    string out = "";
    string color = this->color;
    if(this->error){
        color += "background-image: repeating-linear-gradient(120deg, transparent, transparent 1.40em, #A85860 1.40em, #A85860 2.80em);";
    }

    out+="<div class=\"block normal\" style=\"width: ";
    out+= IntToString(unit_width * width);
    out+="em;";
    out+=color;
    out+=";\">";
    out+="<strong>";
    out+= ADDRINTToString(this->start());
    out+="</strong><br />";
    out+="+ ";
    out+= ADDRINTToString(this->end() - this->start());
    out+=" (";
    out+= ADDRINTToString(this->size);
    out+=")";

    out+="</div>\n";
    return out;
}


class State
{
public:
    State(vector<Block>* old_blocks);
    ~State();
    set<ADDRINT>* boundaries();
    vector<Block>* blocks;
    string errors;
    string info;
    ADDRINT toFree;
    ADDRINT toRealloc;
};

State::State(vector<Block>* old_blocks)
{
    this->blocks = new vector<Block>();
    if(!old_blocks->empty()){
        this->blocks->insert(this->blocks->end(),old_blocks->begin(),old_blocks->end());
    }
    this->toFree = 0;
    this->toRealloc = 0;
    this->info = "";
    this->errors = "";
}

State::~State()
{

}

set<ADDRINT>* State::boundaries()
{
    set<ADDRINT>* bounds = new set<ADDRINT>();
    for(vector<Block>::iterator block = this->blocks->begin(); block != this->blocks->end(); ++block){
        bounds->insert(block->start());
        bounds->insert(block->end());
    }
    printf("Fetched bounderies - size:%lu\n",bounds->size());
    return bounds;
}


class Empty
{
public:
    Empty();
    ADDRINT start;
    ADDRINT end;
    bool display;
    string gen_html(int);
};

Empty::Empty(){

}

string Empty::gen_html(int width){
    string out = "<div class=\"";
    out += "block empty\" style=\"width: ";
    out += IntToString(10);
    out+="em; ";
    out+=";\">";
    out+= "<strong>";
    out+=ADDRINTToString(this->start);
    out+="</strong><br />";
    out += "+ " + ADDRINTToString(this->end - this->start);
    out+="</div>";
    return out;
}
/* ===================================================================== */
/* Commandline Switches */
/* ===================================================================== */

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "malloctrace.out", "specify trace file name");

/* ===================================================================== */

VOID MatchPtr(State state, ADDRINT addr,int *s, Block* match)
{
    
    match = NULL;
    s = NULL;
    if(!addr){
        //set everything NULL
        return;
    }

    int i = 0;
    for(vector<Block>::iterator block = state.blocks->begin(); block != state.blocks->end(); ++block){
        if(block->addr == addr){
            if(match == NULL or match->size >= block->size){
                match = &(*block);
                *s = i;
            }
        }
        i++;
    }

    if(!match){
        //error
    }

}

/* ===================================================================== */
/* Analysis routines                                                     */
/* ===================================================================== */

VOID update_boundaries(State* state){
    set<ADDRINT>* bounds = state->boundaries();
    boundaries.insert(bounds->begin(),bounds->end());
}

VOID BeforeMalloc(CHAR * name, ADDRINT size)
{
    printf("Before malloc\n");
    State* state = new State(timeline.back().blocks);
    Block* b = new Block();
    b->size = size;
    state->blocks->push_back(*b);
    timeline.push_back(*state);
}

VOID BeforeFree(CHAR * name, ADDRINT addr)
{
    printf("Before free\n");
    State* state = new State(timeline.back().blocks);
    state->toFree = addr;
    timeline.push_back(*state);

}

VOID AfterFree(ADDRINT ret)
{
    printf("After free\n");
    State* state = &timeline.back();
    state->info.append("free(" + ADDRINTToString(state->toFree) +") = " + ADDRINTToString(ret));
    if(state->toFree == 0){
        timeline.erase(timeline.end());
        return;
    }
    Block* match = NULL;
    int* s = NULL;
    MatchPtr(*state,state->toFree,s,match);
    if(!match){
        state->toFree = 0;
        return;
    }
    if(!ret){
        //error
    }
    state->toFree = 0;
    state->blocks->erase(state->blocks->begin()+*s);
    update_boundaries(state);
}

VOID BeforeCalloc(CHAR * name, ADDRINT num, ADDRINT size)
{
    printf("Before calloc\n");
    vector<Block>* blocks = timeline.back().blocks;
    State* state = new State(blocks);
    Block* b = new Block();
    b->size = num * size;
    state->blocks->push_back(*b);
    timeline.push_back(*state);
}

VOID MallocAfter(ADDRINT ret)
{
    printf("After malloc\n");
    State* state = &timeline.back();
    Block *b = &(state->blocks->back());
    if(!ret){
         state->blocks->erase(state->blocks->end());
    } else {
        b->addr = ret;
    }
    state->info.append("malloc(" + ADDRINTToString(b->size) +") = " + ADDRINTToString(ret));
    update_boundaries(state);
}

VOID CallocAfter(ADDRINT ret)
{
    printf("After calloc\n");
    State* state = &timeline.back();
    Block *b = &(state->blocks->back());
    if(!ret){
         state->blocks->erase(state->blocks->end());
    } else {
        b->addr = ret;
    }
    state->info.append("calloc(" + ADDRINTToString(b->size) +") = " + ADDRINTToString(ret));
    update_boundaries(state);
}

VOID BeforeRealloc(CHAR * name, ADDRINT addr, ADDRINT size)
{
    printf("Before realloc\n");
    if(!addr){ //effectively a malloc
        BeforeMalloc(name,size);
    } else if(!size){ //effectively a free
        BeforeFree(name,addr);
    } else {
        State* state = new State(timeline.back().blocks);
        timeline.push_back(*state);
        state->toRealloc = addr;
        Block* block = new Block();
        block->size = size;
        state->blocks->push_back(*block);
    }
}

VOID ReallocAfter(ADDRINT ret)
{
    printf("After realloc\n");
    State* state = &timeline.back();
    if(!state->toRealloc){
        if(!state->toFree){
            MallocAfter(ret);
        } else {
            AfterFree(ret);
        }
    } else {
        Block* match = NULL;
        int* s = NULL;
        MatchPtr(*state,state->toRealloc,s,match);
        state->toRealloc = 0;
        if(!match) return;
        if(!ret){

        } else {
            Block* block = &(state->blocks->back());
            block->addr = ret;
            Block newBlock = state->blocks->at(*s);
            newBlock.size = block->size;
            newBlock.addr=ret;
            state->blocks->erase(state->blocks->end());
            state->info.append("realloc(" + ADDRINTToString(newBlock.size) +") = " + ADDRINTToString(ret));
        }
        update_boundaries(state);
    }
}

/* ===================================================================== */
/* Instrumentation routines                                              */
/* ===================================================================== */
   
VOID Image(IMG img, VOID *v)
{
    // Instrument the malloc() and free() functions.  Print the input argument
    // of each malloc() or free(), and the return value of malloc().
    //
    //  Find the malloc() function.
    RTN mallocRtn = RTN_FindByName(img, MALLOC);
    if (RTN_Valid(mallocRtn))
    {
        RTN_Open(mallocRtn);

        // Instrument malloc() to print the input argument value and the return value.
        RTN_InsertCall(mallocRtn, IPOINT_BEFORE, (AFUNPTR)BeforeMalloc,
                       IARG_ADDRINT, MALLOC,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_END);
        RTN_InsertCall(mallocRtn, IPOINT_AFTER, (AFUNPTR)MallocAfter,
                       IARG_FUNCRET_EXITPOINT_VALUE, IARG_END);

        RTN_Close(mallocRtn);
    }

    // Find the free() function.
    RTN freeRtn = RTN_FindByName(img, FREE);
    if (RTN_Valid(freeRtn))
    {
        RTN_Open(freeRtn);
        // Instrument free() to print the input argument value.
        RTN_InsertCall(freeRtn, IPOINT_BEFORE, (AFUNPTR)BeforeFree,
                       IARG_ADDRINT, FREE,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_END);
        RTN_InsertCall(freeRtn, IPOINT_AFTER, (AFUNPTR)AfterFree,
                       IARG_FUNCRET_EXITPOINT_VALUE, IARG_END);
        RTN_Close(freeRtn);
    }

    //Find the calloc() function
    RTN callocRtn = RTN_FindByName(img, CALLOC);
    if (RTN_Valid(callocRtn))
    {
        RTN_Open(callocRtn);

        // Instrument callocRtn to print the input argument value and the return value.
        RTN_InsertCall(callocRtn, IPOINT_BEFORE, (AFUNPTR)BeforeCalloc,
                       IARG_ADDRINT, CALLOC,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                       IARG_END);
        RTN_InsertCall(callocRtn, IPOINT_AFTER, (AFUNPTR)CallocAfter,
                       IARG_FUNCRET_EXITPOINT_VALUE, IARG_END);

        RTN_Close(callocRtn);
    }
    //Find the realloc() function
    RTN reallocRtn = RTN_FindByName(img, REALLOC);
    if (RTN_Valid(reallocRtn))
    {
        RTN_Open(reallocRtn);

        // Instrument malloc() to print the input argument value and the return value.
        RTN_InsertCall(reallocRtn, IPOINT_BEFORE, (AFUNPTR)BeforeRealloc,
                       IARG_ADDRINT, REALLOC,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                       IARG_END);
        RTN_InsertCall(reallocRtn, IPOINT_AFTER, (AFUNPTR)ReallocAfter,
                       IARG_FUNCRET_EXITPOINT_VALUE, IARG_END);

        RTN_Close(reallocRtn);
    }
}

/* ===================================================================== */

VOID Fini(INT32 code, VOID *v)
{
    generate_html();
    TraceFile.close();
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */
   
INT32 Usage()
{
    cerr << "This tool produces a trace of calls to malloc." << endl;
    cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

void print_state(State state)
{
    TraceFile << "<div class=\"state ";
    if(state.errors.size()){
        TraceFile << "error";
    }
    TraceFile << "\">\n" << endl;

    set<int> known_stops;

    vector<Block> todo;
    todo.insert(todo.end(),state.blocks->begin(),state.blocks->end());

    TraceFile << "<div class=\"line\" style=\"\">\n" << endl;
    Block* current = NULL;
    int i = 0;
    int last = 0;

    for(set<ADDRINT>::iterator b = boundaries.begin(); b!=boundaries.end();++b){
        if(!current){
            for(vector<Block>::iterator block = state.blocks->begin();block != state.blocks->end();++block){
                if(block->start() == *b){
                    current = &(*block);
                    if(last != i){
                        Empty* emp = new Empty();
                        set<ADDRINT>::iterator it = boundaries.begin();
                        advance(it, last);
                        emp->start = *it;
                        emp->end = *b;
                        TraceFile << emp->gen_html(i - last) << endl;
                        delete emp;
                        last = i;
                    }
                }
            }
        }
        if(!current){
            //don't care
        }else if(current->end() == *b){
            TraceFile << current->gen_html(i - last) << endl;
            last = i;
            current = NULL;
        }
        i++;
    }
    TraceFile << "</div>\n" << endl;
    TraceFile << "<div class=\"log\">" << endl;
    //TODO html escape
    TraceFile << "<p>" << state.info << "</p>" << endl;
    
    TraceFile << "<p>" << state.errors << "</p>" << endl;

    TraceFile << "</div>\n" << endl;

    TraceFile << "</div>\n" << endl;
}
void generate_html()
{
    TraceFile << "<style>" << endl;
    TraceFile << "body {\n"
"font-size: 12px;\n"
"background-color: #EBEBEB;\n"
"font-family: \"Lucida Console\", Monaco, monospace;\n"
"width: "
<< IntToString((boundaries.size() - 1) * (unit_width+1)) <<
"em;\n"
"}"
<< endl;

    TraceFile << "p {\n"
"margin: 0.8em 0 0 0.1em;\n"
"}" << endl;

    TraceFile << ".block {\n"
"float: left;\n"
"padding: 0.5em 0;\n"
"text-align: center;\n"
"color: black;\n"
"}" << endl;

    TraceFile << ".normal {\n"
"-webkit-box-shadow: 2px 2px 4px 0px rgba(0,0,0,0.80);\n"
"-moz-box-shadow: 2px 2px 4px 0px rgba(0,0,0,0.80);\n"
"box-shadow: 2px 2px 4px 0px rgba(0,0,0,0.80);\n"
"}" << endl;

    TraceFile << ".empty + .empty {\n"
"border-left: 1px solid gray;\n"
"margin-left: -1px;\n"
"}" << endl;

    TraceFile << ".empty {\n"
"color: gray;\n"
"}" << endl;

    TraceFile << ".line {  }" << endl;

    TraceFile << ".line:after {\n"
  "content:\"\";\n"
  "display:table;\n"
  "clear:both;\n"
"}" << endl;

    TraceFile << ".state {\n"
"margin: 0.5em; padding: 0;\n"
"background-color: white;\n"
"border-radius: 0.3em;\n"
"-webkit-box-shadow: inset 2px 2px 4px 0px rgba(0,0,0,0.80);\n"
"-moz-box-shadow: inset 2px 2px 4px 0px rgba(0,0,0,0.80);\n"
"box-shadow: inset 2px 2px 4px 0px rgba(0,0,0,0.80);\n"
"padding: 0.5em;\n"
"}" << endl;

    TraceFile << ".log {"
"}" << endl;

    TraceFile << ".error {"
"color: white;\n"
"background-color: #8b1820;\n"
"}" << endl;

    TraceFile << ".error .empty {\n"
"color: white;\n"
"}" << endl;

    TraceFile << "</style>\n" << endl;

    TraceFile << "<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/2.1.3/jquery.min.js\"></script>" << endl;
    TraceFile << "<script>"
"var scrollTimeout = null;\n"
"$(window).scroll(function(){\n"
    "if (scrollTimeout) clearTimeout(scrollTimeout);\n"
    "scrollTimeout = setTimeout(function(){\n"
    "$('.log').stop();\n"
    "$('.log').animate({\n"
     "   'margin-left': $(this).scrollLeft()\n"
    "}, 100);\n"
    "}, 200);\n"
"});\n"
"</script>"<< endl;

    TraceFile << "<body>\n" << endl;

    TraceFile << "<div class=\"timeline\">\n" << endl;
    printf("Timeline length:%lu\n",timeline.size());
    for(vector<State>::const_iterator state = timeline.begin(); state != timeline.end(); ++state){
        print_state(*state);
    }

    TraceFile << "</div>\n" << endl;

    TraceFile << "</body>\n" << endl;

}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[])
{
    // Initialize pin & symbol manager
    PIN_InitSymbols();
    if( PIN_Init(argc,argv) )
    {
        return Usage();
    }
    
    // Write to a file since cout and cerr maybe closed by the application
    TraceFile.open(KnobOutputFile.Value().c_str());
    TraceFile << hex;
    TraceFile.setf(ios::showbase);
    vector<Block>* blocks = new std::vector<Block>();
    State* initial = new State(blocks);
    timeline.push_back(*initial);
    // Register Image to be called to instrument functions.
    IMG_AddInstrumentFunction(Image, 0);
    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();
    
    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
