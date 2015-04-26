#include "pin.H"
#include <iostream>
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
    string get_color();
    int header;
    int footer;
    int round;
    int minsz;
    bool error;
    bool details;
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
    this->details = false;
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
    return "";
}

string Block::gen_html(int width)
{
    string out = "";
    string color = this->get_color();
    if(this->error){
        color += "background-image: repeating-linear-gradient(120deg, transparent, transparent 1.40em, #A85860 1.40em, #A85860 2.80em);";
    }

    out+="<div class=\"block normal\" style=\"width: ";
    out+= 10 * width;
    out+=";";
    out+=color;
    out+=" %s;\">";
    if(this->details){
        out+="<strong>";
        out+= this->start();
        out+="</strong><br />";
        out+="+ ";
        out+= (this->end() - this->start());
        out+=" (";
        out+= this->size;
        out+=")";
    } else {
        out+="&nbsp;";
    }

    out+="</div>\n";
    return out;
}


class State
{
public:
    State(vector<Block> blocks);
    ~State();
    set<ADDRINT> boundaries();
    vector<Block> blocks;
    vector<string> errors;
    vector<string> info;
    ADDRINT toFree;
    ADDRINT toRealloc;
};

State::State(vector<Block> blocks)
{
    this->blocks = blocks;
    this->toFree = 0;
    this->toRealloc = 0;
}

set<ADDRINT> State::boundaries()
{
    set<ADDRINT> bounds;
    for(vector<Block>::iterator block = this->blocks.begin(); block != this->blocks.end(); ++block){
        bounds.insert(block->start());
        bounds.insert(block->end());
    }
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
    string color = "???";
    string out = "<div class=\"";
    out += "block empty\" style=\"width: ";
    out += 10 * width;
    out+="; ";
    out+=color;
    out+=";\">";
    out+= "<strong>";
    out+=this->start;
    out+="</strong><br />";
    out += "+ " + (this->end - this->start);
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
    for(vector<Block>::iterator block = state.blocks.begin(); block != state.blocks.end(); ++block){
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
    set<ADDRINT> bounds = state->boundaries();
    boundaries.insert(bounds.begin(),bounds.end());
}

VOID BeforeMalloc(CHAR * name, ADDRINT size)
{
    State* state = new State(timeline.end()->blocks);
    Block* b = new Block();
    b->size = size;
    state->blocks.push_back(*b);
    timeline.push_back(*state);
}

VOID BeforeFree(CHAR * name, ADDRINT addr)
{
    State* state = new State(timeline.end()->blocks);
    if(!addr){
        return;
    } 
    state->toFree = addr;
    timeline.push_back(*state);

}

VOID AfterFree(ADDRINT ret)
{
    State* state = &timeline.back();
    Block* match = NULL;
    int* s = NULL;
    MatchPtr(*state,state->toFree,s,match);

    if(!match){
        return;
    }
    if(!ret){
        //error
    }
    state->toFree = 0;
    state->blocks.erase(state->blocks.begin()+*s);
    update_boundaries(state);
}

VOID BeforeCalloc(CHAR * name, ADDRINT num, ADDRINT size)
{
    State* state = new State(timeline.end()->blocks);
    Block* b = new Block();
    b->size = num * size;
    state->blocks.push_back(*b);
    timeline.push_back(*state);
}

VOID MallocAfter(ADDRINT ret)
{
    State* state = &timeline.back();
    Block *b = &(state->blocks.back());
    if(!ret){
         state->blocks.erase(state->blocks.end());
    } else {
        b->addr = ret;
    }
    update_boundaries(state);
}

VOID CallocAfter(ADDRINT ret)
{
    State* state = &timeline.back();
    Block *b = &(state->blocks.back());
    if(!ret){
         state->blocks.erase(state->blocks.end());
    } else {
        b->addr = ret;
    }
    update_boundaries(state);
}

VOID BeforeRealloc(CHAR * name, ADDRINT addr, ADDRINT size)
{
    if(!addr){ //effectively a malloc
        BeforeMalloc(name,size);
    } else if(!size){ //effectively a free
        BeforeFree(name,addr);
    } else {
        State* state = new State(timeline.end()->blocks);
        timeline.push_back(*state);
        state->toRealloc = addr;
        Block* block = new Block();
        block->size = size;
        state->blocks.push_back(*block);
    }
}

VOID ReallocAfter(ADDRINT ret)
{
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
        if(!match) return;
        if(!ret){

        } else {
            Block* block = &(state->blocks.back());
            block->addr = ret;
            Block newBlock = state->blocks.at(*s);
            newBlock.size = block->size;
            newBlock.addr=ret;
            state->blocks.erase(state->blocks.end());
            state->toRealloc = 0;
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
    while(todo.size()){

         TraceFile << "<div class=\"line\" style=\"\">\n" << endl;

        vector<Block> done;

        Block* current = NULL;
        int last = 0;
        int i = -1;
        for(set<ADDRINT>::iterator b = boundaries.begin(); b != boundaries.end(); ++b){
            i++;
            // If this block has size 0; make it continue until the
            // next boundary anyway. The size will be displayed as
            //0 or unknown anyway and it shouldn't be too confusing.
            if(current && current->end() != *b && current->start() != current->end()){
                continue;
            }

            if(current){  // stops here.
                known_stops.insert(i);
                current->gen_html(i - last);
                done.push_back(*current);
                last = i;
            }

            current = NULL;
            for(vector<Block>::iterator block = state.blocks.begin(); block != state.blocks.end(); ++block){
                if(block->start() == *b){
                    current = &(*block);
                    break;
                }
            }
            if(!current) continue;

            if(last != i){

                // We want to show from previous known_stop.
                for(int s = i+1;s>last;s--){
                    if(known_stops.find(s) == known_stops.end()){
                        break;
                    }
                    if(s != last){
                        Empty emp = Empty();
                        set<ADDRINT>::iterator it = boundaries.begin();
                        advance(it, last);
                        emp.start = *it;
                        it = boundaries.begin();
                        advance(it, s);
                        emp.end = *it;
                        TraceFile << emp.gen_html(s - last) << endl;
                        known_stops.insert(s);
                    }

                    if(s != i){
                        Empty emp = Empty();
                        set<ADDRINT>::iterator it = boundaries.begin();
                        advance(it, s);
                        emp.start = *it;
                        emp.end = *b;
                        TraceFile << emp.gen_html(i - s) << endl;
                        known_stops.insert(i);
                    }
                }
                last = i;
            }


        if(current){
            throw runtime_error("Block was started but never finished.");
        }

        if(!done.size()){
            throw runtime_error("Some block(s) don't match boundaries.");
        }

        TraceFile << "</div>\n" << endl;
        vector<Block> tmp;
        for(vector<Block>::iterator x = todo.begin(); x != todo.end(); ++x){
            for(vector<Block>::iterator y = done.begin(); y != done.end(); ++y){
                if(y->addr == x->addr && y->size == x->size){
                    tmp.push_back(*x);
                }
            }
        }
        todo = tmp;
    }
    TraceFile << "<div class=\"log\">" << endl;
    //TODO html escape
    for(vector<string>::const_iterator msg = state.info.begin(); msg != state.info.end(); ++msg){
        TraceFile << "<p>" << *msg << "</p>" << endl;
    }
    
    for(vector<string>::const_iterator msg = state.errors.begin(); msg != state.errors.end(); ++msg){
        TraceFile << "<p>" << *msg << "</p>" << endl;
    }

     TraceFile << "</div>\n" << endl;

     TraceFile << "</div>\n" << endl;
}
}
void generate_html()
{
    TraceFile << "<style>" << endl;
    TraceFile << "body {"
"font-size: 12px;"
"background-color: #EBEBEB;"
"font-family: \"Lucida Console\", Monaco, monospace;"
"width: "
<< ((boundaries.size() - 1) * (unit_width + 1)) <<
"dem;"
"}"
<< endl;

    TraceFile << "p {"
"margin: 0.8em 0 0 0.1em;"
"}" << endl;

    TraceFile << ".block {"
"float: left;"
"padding: 0.5em 0;"
"text-align: center;"
"color: black;"
"}" << endl;

    TraceFile << ".normal {"
"-webkit-box-shadow: 2px 2px 4px 0px rgba(0,0,0,0.80);"
"-moz-box-shadow: 2px 2px 4px 0px rgba(0,0,0,0.80);"
"box-shadow: 2px 2px 4px 0px rgba(0,0,0,0.80);"
"}" << endl;

    TraceFile << ".empty + .empty {"
"border-left: 1px solid gray;"
"margin-left: -1px;"
"}" << endl;

    TraceFile << ".empty {"
"color: gray;"
"}" << endl;

    TraceFile << ".line {  }" << endl;

    TraceFile << ".line:after {"
  "content:\"\";"
  "display:table;"
  "clear:both;"
"}" << endl;

    TraceFile << ".state {"
"margin: 0.5em; padding: 0;"
"background-color: white;"
"border-radius: 0.3em;"
"-webkit-box-shadow: inset 2px 2px 4px 0px rgba(0,0,0,0.80);"
"-moz-box-shadow: inset 2px 2px 4px 0px rgba(0,0,0,0.80);"
"box-shadow: inset 2px 2px 4px 0px rgba(0,0,0,0.80);"
"padding: 0.5em;"
"}" << endl;

    TraceFile << ".log {"
"}" << endl;

    TraceFile << ".error {"
"color: white;"
"background-color: #8b1820;"
"}" << endl;

    TraceFile << ".error .empty {"
"color: white;"
"}" << endl;

    TraceFile << "</style>\n" << endl;

    TraceFile << "<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/2.1.3/jquery.min.js\"></script>" << endl;
    TraceFile << "<script>"
"var scrollTimeout = null;"
"$(window).scroll(function(){"
    "if (scrollTimeout) clearTimeout(scrollTimeout);"
    "scrollTimeout = setTimeout(function(){"
    "$('.log').stop();"
    "$('.log').animate({"
     "   'margin-left': $(this).scrollLeft()"
    "}, 100);"
    "}, 200);"
"});"
"</script>"<< endl;

    TraceFile << "<body>\n" << endl;

    TraceFile << "<div class=\"timeline\">\n" << endl;

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
    vector<Block> blocks;
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
