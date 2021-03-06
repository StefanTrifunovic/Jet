#ifdef _DEBUG
#ifndef DBG_NEW      
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )     
#define new DBG_NEW   
#endif

#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "Compiler.h"
#include "Parser.h"

using namespace Jet;

CompilerContext::CompilerContext(void)
{
	this->vararg = false;
	this->closures = 0;
	this->parent = 0;
	this->uuid = 0;
	this->localindex = 0;
	this->lastline = 0;
	this->scope = new CompilerContext::Scope;
	this->scope->level = 0;
	this->scope->previous = this->scope->next = 0;
}

CompilerContext::~CompilerContext(void)
{
	if (this->scope)
	{
		auto next = this->scope->next;
		while (next)
		{
			auto tmp = next->next;
			delete next;
			next = tmp;
		}
	}
	delete this->scope;

	//delete functions
	for (auto ii: this->functions)
		delete ii.second;
}

void CompilerContext::PrintAssembly()
{
	int index = 0;
	for (int i = 0; i < this->out.size(); i++)
	{
		auto ins = this->out[i];
		if (ins.type == InstructionType::Function)
		{
			printf("\n\nfunction %s definition\n%d arguments, %d locals, %d captures", ins.string, ins.a, ins.b, ins.c);
			index = 0;

			if (i+1 < this->out.size() && out[i+1].type == InstructionType::DebugLine)
			{
				++i;
				printf(", %s line %.0lf", out[i].string, out[i].second);
			}
			continue;
		}

		if (ins.string)
			printf("\n[%d]\t%-15s %-5.0lf %s", index++, Instructions[(int)ins.type], ins.second, ins.string);
		else
			printf("\n[%d]\t%-15s %-5d %.0lf", index++, Instructions[(int)ins.type], ins.first, ins.second);

		if (i+1 < this->out.size() && out[i+1].type == InstructionType::DebugLine)
		{
			++i;
			printf(" ; %s line %.0lf", out[i].string, out[i].second);
		}
	}
	printf("\n");
}

std::vector<IntermediateInstruction> CompilerContext::Compile(BlockExpression* expr)
{
	try
	{
		//may want to correct number of locals here
		this->FunctionLabel("{Entry Point}", 0, 0, 0, this->vararg);

		expr->Compile(this);

		//add a return to signify end of global code
		this->Return();

		this->Compile();

		if (localindex > 255)
			throw CompilerException(this->lastfile, this->lastline, "Too many locals: over 256 locals in function!");
		if (closures > 255)
			throw CompilerException(this->lastfile, this->lastline, "Too many closures: over 256 closures in function!");

		//modify the entry point with number of locals
		this->out[0].b = this->localindex;
		this->out[0].c = this->closures;
	}
	catch (CompilerException e)
	{
		//clean up the compiler
		if (this->scope)
		{
			auto next = this->scope->next;
			this->scope->next = 0;
			while (next)
			{
				auto tmp = next->next;
				delete next;
				next = tmp;
			}
		}
		this->scope->localvars.clear();

		for (auto ii: this->functions)
			delete ii.second;

		this->functions.clear();

		this->localindex = 0;
		this->closures = 0;

		throw e;
	}

	if (this->scope)
	{
		auto next = this->scope->next;
		this->scope->next = 0;
		while (next)
		{
			auto tmp = next->next;
			delete next;
			next = tmp;
		}
		this->scope->localvars.clear();
	}

	for (auto ii: this->functions)
		delete ii.second;

	this->functions.clear();
	//add metamethods and custom operators
	this->localindex = 0;
	this->closures = 0;

	//this->PrintAssembly();

	auto temp = std::move(this->out);
	this->out.clear();
	return std::move(temp);
}

bool CompilerContext::RegisterLocal(const std::string name)
{
	//neeed to store locals in a contiguous array, even with different scopes
	for (unsigned int i = 0; i < this->scope->localvars.size(); i++)
	{
		if (this->scope->localvars[i].name == name)
			return false;
	}

	LocalVariable var;
	var.local = this->localindex++;
	var.name = name;
	var.capture = -1;
	this->scope->localvars.push_back(var);
	return true;
}

void CompilerContext::BinaryOperation(TokenType operation)
{
	switch (operation)
	{
	case TokenType::Plus:
	case TokenType::AddAssign:
		this->out.push_back(IntermediateInstruction(InstructionType::Add));
		break;	
	case TokenType::Asterisk:
	case TokenType::MultiplyAssign:
		this->out.push_back(IntermediateInstruction(InstructionType::Mul));
		break;	
	case TokenType::Minus:
	case TokenType::SubtractAssign:
		this->out.push_back(IntermediateInstruction(InstructionType::Sub));
		break;
	case TokenType::Slash:
	case TokenType::DivideAssign:
		this->out.push_back(IntermediateInstruction(InstructionType::Div));
		break;
	case TokenType::Modulo:
		this->out.push_back(IntermediateInstruction(InstructionType::Modulus));
		break;
	case TokenType::Equals:
		this->out.push_back(IntermediateInstruction(InstructionType::Eq));
		break;
	case TokenType::NotEqual:
		this->out.push_back(IntermediateInstruction(InstructionType::NotEq));
		break;
	case TokenType::LessThan:
		this->out.push_back(IntermediateInstruction(InstructionType::Lt));
		break;
	case TokenType::GreaterThan:
		this->out.push_back(IntermediateInstruction(InstructionType::Gt));
		break;
	case TokenType::LessThanEqual:
		this->out.push_back(IntermediateInstruction(InstructionType::LtE));
		break;
	case TokenType::GreaterThanEqual:
		this->out.push_back(IntermediateInstruction(InstructionType::GtE));
		break;
	case TokenType::Or:
		this->out.push_back(IntermediateInstruction(InstructionType::BOr));
		break;
	case TokenType::And:
		this->out.push_back(IntermediateInstruction(InstructionType::BAnd));
		break;
	case TokenType::Xor:
		this->out.push_back(IntermediateInstruction(InstructionType::Xor));
		break;
	case TokenType::LeftShift:
		this->out.push_back(IntermediateInstruction(InstructionType::LeftShift));
		break;
	case TokenType::RightShift:
		this->out.push_back(IntermediateInstruction(InstructionType::RightShift));
		break;
	}
}

void CompilerContext::UnaryOperation(TokenType operation)
{
	switch (operation)
	{
	case TokenType::Increment:
		this->out.push_back(IntermediateInstruction(InstructionType::Incr));
		break;
	case TokenType::Decrement:
		this->out.push_back(IntermediateInstruction(InstructionType::Decr));
		break;	
	case TokenType::Minus:
		this->out.push_back(IntermediateInstruction(InstructionType::Negate));
		break;
	case TokenType::BNot:
		this->out.push_back(IntermediateInstruction(InstructionType::BNot));
		break;
	}
}

CompilerContext* CompilerContext::AddFunction(std::string name, unsigned int args, bool vararg)
{
	//push instruction that sets the function
	//todo, may need to have functions in other instruction code sets
	CompilerContext* newfun = new CompilerContext();
	//insert this into my list of functions
	std::string fname = name+this->GetUUID();
	newfun->arguments = args;
	newfun->uuid = this->uuid;
	newfun->parent = this;
	newfun->vararg = vararg;
	this->functions[fname] = newfun;

	//store the function in the variable
	this->LoadFunction(fname);

	return newfun;
};

void CompilerContext::FinalizeFunction(CompilerContext* c)
{
	this->uuid = c->uuid + 1;

	//move upvalues
	int level = 0;
	auto ptr = this->scope;
	while (ptr)
	{
		//make sure this doesnt upload multiple times
		//look for var in locals
		for (unsigned int i = 0; i < ptr->localvars.size(); i++)
		{
			if (ptr->localvars[i].capture >= 0 && ptr->localvars[i].uploaded == false)
			{
				//printf("We found use of a captured var: %s at level %d, index %d\n", ptr->localvars[i].second.c_str(), level, ptr->localvars[i].first);
				//exit the loops we found it
				ptr->localvars[i].uploaded = true;
				//this->output += ".local " + variable + " " + ::std::to_string(i) + ";\n";
				out.push_back(IntermediateInstruction(InstructionType::CInit,ptr->localvars[i].local, ptr->localvars[i].capture));
				//push instruction to set closure location
				//out.push_back(IntermediateInstruction(InstructionType::LLoad, ptr->localvars[i].first, 0));//i, ptr->level));
				//out.push_back(IntermediateInstruction(InstructionType::CStore, ptr->localvars[i].third, level));//i, ptr->level));
			}
		}
		if (ptr)
			ptr = ptr->previous;
	}
}
