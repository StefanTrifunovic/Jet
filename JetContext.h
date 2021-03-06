#ifndef _LANG_CONTEXT_HEADER
#define _LANG_CONTEXT_HEADER

#include <functional>
#include <string>
#include <map>
#include <algorithm>

#include "Value.h"

#include "VMStack.h"
#include "Parser.h"
#include "JetInstructions.h"
#include "JetExceptions.h"
#include "GarbageCollector.h"

#ifdef _DEBUG
#ifndef DBG_NEW      
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )     
#define new DBG_NEW   
#endif

#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#ifdef _WIN32
#include <Windows.h>
//#define JET_TIME_EXECUTION
#endif

#define GC_INTERVAL 100//number of allocations before running the GC
#define GC_STEPS 4//number of incremental runs before a full

#define JET_STACK_SIZE 800
#define JET_MAX_CALLDEPTH 400
//use the JetArray
namespace Jet
{
	typedef std::function<void(Jet::JetContext*,Jet::Value*,int)> JetFunction;
#define JetBind(context, fun) 	auto temp__bind_##fun = [](Jet::JetContext* context,Jet::Value* args, int numargs) { context->Return(fun(args[0]));};context[#fun] = Jet::Value(temp__bind_##fun);
	//void(*temp__bind_##fun)(Jet::JetContext*,Jet::Value*,int)> temp__bind_##fun = &[](Jet::JetContext* context,Jet::Value* args, int numargs) { context->Return(fun(args[0]));}; context[#fun] = &temp__bind_##fun;
#define JetBind2(context, fun) 	auto temp__bind_##fun = [](Jet::JetContext* context,Jet::Value* args, int numargs) { context->Return(fun(args[0],args[1]));};context[#fun] = Jet::Value(temp__bind_##fun);
#define JetBind2(context, fun, type) 	auto temp__bind_##fun = [](Jet::JetContext* context,Jet::Value* args, int numargs) { context->Return(fun((type)args[0],(type)args[1]));};context[#fun] = Jet::Value(temp__bind_##fun);

	struct Instruction
	{
		InstructionType instruction;
		union
		{
			int value;
			Function* func;
		};
		union
		{
			double value2;
			//const char* string;
		};
		const char* string;
	};

	//builtin function definitions
	void gc(JetContext* context,Value* args, int numargs);
	void print(JetContext* context,Value* args, int numargs);

	class JetContext
	{
		friend class GarbageCollector;
		VMStack<Value> stack;
		VMStack<std::pair<unsigned int, Closure*> > callstack;

		//need to go through and find all labels/functions/variables
		//fix the labels, should only work with labels in the same function
		//not just accumulate over the lifetime, so this should be cleared
		//every function instruction
		std::map<std::string, unsigned int> labels;
		std::map<std::string, Function*> functions;
		std::vector<Function*> entrypoints;
		std::map<std::string, unsigned int> variables;//mapping from string to location in vars array

		//debug info
		struct DebugInfo
		{
			unsigned int code;
			std::string file;
			unsigned int line;
		};
		std::vector<DebugInfo> debuginfo;

		//actual data being worked on
		std::vector<Instruction> ins;
		std::vector<Value> vars;//where they are actually stored

		int labelposition;//used for keeping track in assembler
		CompilerContext compiler;//root compiler context

		//core library prototypes
		_JetObject string;
		_JetObject Array;
		_JetObject object;
		_JetObject file;
		_JetObject arrayiter;
		_JetObject objectiter;

	private:
		//garbage collector stuff
		//std::vector<_JetArray*> arrays;
		//std::vector<_JetObject*> objects;
		//std::vector<GCVal<char*>*> strings;
		//std::vector<_JetUserdata*> userdata;
		//std::vector<Closure*> closures;

		//manages memory
		GarbageCollector gc;

		//int allocationCounter;//used to determine when to run the GC
		//int collectionCounter;
		//VMStack<Value> greys;//stack of grey objects for processing
	public:

		Value NewObject();
		Value NewArray();
		Value NewUserdata(void* data, const Value& proto);
		Value NewString(char* string, bool copy = true);

		//a helper function for registering metatables, returns an object
		//this doesnt get garbage collected and you must delete it yourself after done using it
		Value NewPrototype(const char* Typename);

		JetContext();
		~JetContext();

		//allows assignment and reading of gobal variables
		Value& operator[](const std::string& id);
		Value Get(const std::string& name);
		void Set(const std::string& name, const Value& value);

		std::string Script(const std::string code, const std::string filename = "file");
		Value Script(const char* code, const char* filename = "file");//compiles, assembles and executes the script


		//compiles source code to ASM for the VM to read in
		std::vector<IntermediateInstruction> Compile(const char* code, const char* filename = "file");

		//xecutes global code and parses in ASM
		Value Assemble(const std::vector<IntermediateInstruction>& code);

		//executes a function in the VM context
		Value Call(const char* function, Value* args = 0, unsigned int numargs = 0);
		Value Call(Value* function, Value* args = 0, unsigned int numargs = 0);

		void RunGC();//runs an iteration of the garbage collector

		void Return(Value val);//returns value from native functions

	private:
		int fptr;//frame pointer, not really used except in gc
		Value* sptr;//stack pointer
		Closure* curframe;
		Value localstack[JET_STACK_SIZE];

		//begin executing instructions at iptr index
		Value Execute(int iptr);

		//debug functions
		void GetCode(int ptr, std::string& ret, unsigned int& line);
		void StackTrace(int curiptr);
	};
}

#endif