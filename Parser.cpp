#include "Parser.h"
#include "Token.h"
#undef EOF

using namespace Jet;
std::map<TokenType,std::string> Jet::TokenToString; 

bool Jet::IsLetter(char c)
{
	return (c >= 'a' && c <= 'z') || ( c >= 'A' && c <= 'Z');
}

bool Jet::IsNumber(char c)
{
	return (c >= '0' && c <= '9');
}

char* Jet::Operator(TokenType t)
{
	if (t == TokenType::Plus)
		return "+";
	else if (t == TokenType::Minus)
		return "-";
	else if (t == TokenType::Asterisk)
		return "*";
	else if (t == TokenType::Slash)
		return "/";
	else if (t == TokenType::Modulo)
		return "%";
	else if (t == TokenType::Comma)
		return ",";
	else if (t == TokenType::Increment)
		return "++";
	else if (t == TokenType::Decrement)
		return "--";
	else if (t == TokenType::Equals)
		return "==";
	else if (t == TokenType::NotEqual)
		return "!=";
	else if (t == TokenType::Semicolon)
		return ";";
	else if (t == TokenType::RightBrace)
		return "}";
	return "";
}

Parser::Parser(Lexer* l)
{
	this->lexer = l;

	this->Register(TokenType::Name, new NameParselet());
	this->Register(TokenType::Number, new NumberParselet());
	this->Register(TokenType::String, new StringParselet());
	this->Register(TokenType::Assign, new AssignParselet());

	this->Register(TokenType::LeftParen, new GroupParselet());

	this->Register(TokenType::Swap, new SwapParselet());

	this->Register(TokenType::Dot, new MemberParselet());
	this->Register(TokenType::LeftBrace, new ObjectParselet());

	//array/index stuffs
	this->Register(TokenType::LeftBracket, new ArrayParselet());
	this->Register(TokenType::LeftBracket, new IndexParselet());//postfix

	//operator assign
	this->Register(TokenType::AddAssign, new OperatorAssignParselet());
	this->Register(TokenType::SubtractAssign, new OperatorAssignParselet());
	this->Register(TokenType::MultiplyAssign, new OperatorAssignParselet());
	this->Register(TokenType::DivideAssign, new OperatorAssignParselet());


	//prefix stuff
	this->Register(TokenType::Increment, new PrefixOperatorParselet(6));
	this->Register(TokenType::Decrement, new PrefixOperatorParselet(6));

	//postfix stuff
	this->Register(TokenType::Increment, new PostfixOperatorParselet(7));
	this->Register(TokenType::Decrement, new PostfixOperatorParselet(7));

	//boolean stuff
	this->Register(TokenType::Equals, new BinaryOperatorParselet(2, false));
	this->Register(TokenType::NotEqual, new BinaryOperatorParselet(2, false));
	this->Register(TokenType::LessThan, new BinaryOperatorParselet(2, false));
	this->Register(TokenType::GreaterThan, new BinaryOperatorParselet(2, false));

	//math
	this->Register(TokenType::Plus, new BinaryOperatorParselet(4, false));
	this->Register(TokenType::Minus, new BinaryOperatorParselet(4, false));
	this->Register(TokenType::Asterisk, new BinaryOperatorParselet(5, false));
	this->Register(TokenType::Slash, new BinaryOperatorParselet(5, false));
	this->Register(TokenType::Modulo, new BinaryOperatorParselet(5, false));
	this->Register(TokenType::Or, new BinaryOperatorParselet(3, false));//or
	this->Register(TokenType::And, new BinaryOperatorParselet(3, false));//and


	//function stuff
	this->Register(TokenType::LeftParen, new CallParselet());

	//lambda
	this->Register(TokenType::Function, new LambdaParselet());

	//statements
	this->Register(TokenType::While, new WhileParselet()); 
	this->Register(TokenType::If, new IfParselet());
	this->Register(TokenType::Function, new FunctionParselet());
	this->Register(TokenType::Ret, new ReturnParselet());
	this->Register(TokenType::For, new ForParselet());
	this->Register(TokenType::Local, new LocalParselet());
}

Parser::~Parser()
{
	for (auto ii: this->mInfixParselets)
		delete ii.second;

	for (auto ii: this->mPrefixParselets)
		delete ii.second;

	for (auto ii: this->mStatementParselets)
		delete ii.second;
};

Expression* Parser::ParseStatement(bool takeTrailingSemicolon)//call this until out of tokens (hit EOF)
{
	Token token = LookAhead();
	StatementParselet* statement = mStatementParselets[token.getType()];

	Expression* result;
	if (statement == 0)
	{
		result = parseExpression();

		if (takeTrailingSemicolon)
			Consume(TokenType::Semicolon);

		return result;
	}

	token = Consume();
	result = statement->parse(this, token);

	if (takeTrailingSemicolon && statement->TrailingSemicolon)
		Consume(TokenType::Semicolon);

	return result;
}

BlockExpression* Parser::parseBlock(bool allowsingle)
{
	std::vector<Expression*>* statements = new std::vector<Expression*>;

	if (allowsingle && !Match(TokenType::LeftBrace))
	{
		statements->push_back(this->ParseStatement());
		return new BlockExpression(statements);
	}

	Consume(TokenType::LeftBrace);

	while (!Match(TokenType::RightBrace))
	{
		statements->push_back(this->ParseStatement());
	}

	Consume(TokenType::RightBrace);
	return new BlockExpression(statements);
}

BlockExpression* Parser::parseAll()
{
	auto statements = new std::vector<Expression*>;
	while (!Match(TokenType::EOF))
	{
		statements->push_back(this->ParseStatement());
	}
	auto n = new BlockExpression(statements);
	n->SetParent(0);//go through and setup parents
	return n;
}

Token Parser::Consume()
{
	LookAhead();

	auto temp = mRead.front();
	mRead.pop_front();
	return temp;//mRead.remove(0);
}

Token Parser::Consume(TokenType expected)
{
	LookAhead();
	auto temp = mRead.front();
	if (temp.getType() != expected)
	{
		std::string str = "Consume: TokenType not as expected! Expected: " + TokenToString[expected] + " Got: " + temp.text;
		throw ParserException(temp.filename, temp.line, str);//printf("Consume: TokenType not as expected!\n");
	}
	mRead.pop_front();
	return temp;
}

Token Parser::LookAhead(int num)
{
	while (num >= mRead.size())
	{
		mRead.push_back(lexer->Next());
	}

	int c = 0;
	for (auto ii: mRead)
	{
		if (c++ == num)
		{
			return ii;
		}
	}

	//return mRead.get(num);
}

bool Parser::Match(TokenType expected)
{
	Token token = LookAhead();
	if (token.getType() != expected)
	{
		return false;
	}

	return true;
}

bool Parser::MatchAndConsume(TokenType expected)
{
	Token token = LookAhead();
	if (token.getType() != expected)
	{
		return false;
	}

	Consume();
	return true;
}

void Parser::Register(TokenType token, InfixParselet* parselet)
{
	this->mInfixParselets[token] = parselet;
}

void Parser::Register(TokenType token, PrefixParselet* parselet)
{
	this->mPrefixParselets[token] = parselet;
}

void Parser::Register(TokenType token, StatementParselet* parselet)
{
	this->mStatementParselets[token] = parselet;
}


Token Lexer::Next()
{
	while (index < text.length())
	{
		char c = this->ConsumeChar();
		std::string str = text.substr(index-1, 1);
		bool found = false; int len = 0;
		for (auto ii: operators)
		{
			if (strncmp(ii.first.c_str(), &text.c_str()[index-1], ii.first.length()) == 0 && ii.first.length() > len)
			{
				len = ii.first.length();
				str = ii.first;
				found = true;
			}
		}

		if (found)
		{
			for (int i = 0; i < len-1; i++)
				this->ConsumeChar();

			std::string s;
			s += c;
			if (operators[str] == TokenType::LineComment)
			{
				//go to next line
				while(this->ConsumeChar() != '\n') {};

				continue;
			}
			else if (operators[str] == TokenType::CommentBegin)
			{
				Token n = this->Next();
				while(n.type != TokenType::CommentEnd) 
				{
					if (n.type == TokenType::EOF)
					{
						throw ParserException(n.filename, n.line, "Missing end to comment block starting at:");
					}
					n = this->Next();
				}
				continue;
			}
			else if (operators[str] == TokenType::String)
			{
				//Token n;
				int start = index;
				while (index < text.length())
				{
					if (text.at(index) == '"')
						break;

					index++;
				}

				std::string txt = text.substr(start, index-start);
				index++;
				return Token("test", linenumber, operators[str], txt);
			}

			return Token("test", linenumber, operators[str], str);
		}
		else if (IsLetter(c))//word
		{
			int start = index - 1;
			while (index < text.length())
			{
				char c = this->PeekChar();
				if (!IsLetter(text.at(index)))
					if (!IsNumber(text.at(index)))
						break;

				this->ConsumeChar();
			}

			std::string name = text.substr(start, index-start);
			//check if it is a keyword
			if (keywords.find(name) != keywords.end())//is keyword?
				return Token("test", linenumber, keywords[name], name);
			else//just a variable name
				return Token("test", linenumber, TokenType::Name, name);
		}
		else if (c >= '0' && c <= '9')//number
		{
			int start = index-1;
			while (index < text.length())
			{
				if (!(text[index] == '.' || (text[index] >= '0' && text[index] <= '9')))
					break;

				this->ConsumeChar();
			}

			std::string num = text.substr(start, index-start);
			return Token("test", linenumber, TokenType::Number, num);
		}
		else
		{
			//character to ignore like whitespace
		}
	}
	return Token("test", linenumber, TokenType::EOF, "");
}

Expression* NameParselet::parse(Parser* parser, Token token)
{
	if (parser->LookAhead().getType() == TokenType::LeftBracket)
	{
		//array index
		parser->Consume();
		Expression* index = parser->parseExpression();
		parser->Consume(TokenType::RightBracket);

		return new IndexExpression(new NameExpression(token.getText()), index);
	}
	else
		return new NameExpression(token.getText());
}

Expression* AssignParselet::parse(Parser* parser, Expression* left, Token token)
{
	Expression* right = parser->parseExpression(/*assignment prcedence -1 */);

	if (dynamic_cast<IStorableExpression*>(left) == 0)
	{
		std::string str = "AssignParselet: Left hand side must be a storable location!";

		throw ParserException(token.filename, token.line, str);//printf("Consume: TokenType not as expected!\n");
	}

	return new AssignExpression(left, right);
}

Expression* OperatorAssignParselet::parse(Parser* parser, Expression* left, Token token)
{
	Expression* right = parser->parseExpression(/*assignment prcedence -1 */);

	if (dynamic_cast<IStorableExpression*>(left) == 0)
	{
		std::string str = "OperatorAssignParselet: Left hand side must be a storable location!";
		throw ParserException(token.filename, token.line, str);//printf("Consume: TokenType not as expected!\n");

		return 0;
	}

	return new OperatorAssignExpression(token, left, right);
}

Expression* SwapParselet::parse(Parser* parser, Expression* left, Token token)
{
	Expression* right = parser->parseExpression(/*assignment prcedence -1 */);

	if (dynamic_cast<IStorableExpression*>(left) == 0)
	{
		std::string str = "SwapParselet: Left hand side must be a storable location!";
		throw ParserException(token.filename, token.line, str);//printf("Consume: TokenType not as expected!\n");
	}

	if (dynamic_cast<IStorableExpression*>(right) == 0)
	{
		std::string str = "SwapParselet: Right hand side must be a storable location!";
		throw ParserException(token.filename, token.line, str);//printf("Consume: TokenType not as expected!\n");
	}

	//std::string name = dynamic_cast<NameExpression*>(left)->GetName();
	return new SwapExpression(left, right);
}

Expression* PrefixOperatorParselet::parse(Parser* parser, Token token)
{
	Expression* right = parser->parseExpression(precedence);
	if (right == 0)
	{
		std::string str = "PrefixOperatorParselet: Right hand side missing!";
		throw ParserException(token.filename, token.line, str);//printf("Consume: TokenType not as expected!\n");
	}
	return new PrefixExpression(token.getType(), right);//::atof(token.getText().c_str()));
}

Expression* BinaryOperatorParselet::parse(Parser* parser, Expression* left, Token token)
{
	Expression* right = parser->parseExpression(precedence - (isRight ? 1 : 0));
	if (right == 0)
	{
		std::string str = "BinaryOperatorParselet: Right hand side missing!";
		throw ParserException(token.filename, token.line, str);//printf("Consume: TokenType not as expected!\n");
	}
	return new OperatorExpression(left, token.getType(), right);//::atof(token.getText().c_str()));
}

Expression* GroupParselet::parse(Parser* parser, Token token)
{
	Expression* exp = parser->parseExpression();
	parser->Consume(TokenType::RightParen);
	return exp;
}

Expression* WhileParselet::parse(Parser* parser, Token token)
{
	parser->Consume(TokenType::LeftParen);

	auto condition = parser->parseExpression();

	parser->Consume(TokenType::RightParen);

	auto block = new ScopeExpression(parser->parseBlock());
	return new WhileExpression(token, condition, block);
}

Expression* ForParselet::parse(Parser* parser, Token token)
{
	parser->Consume(TokenType::LeftParen);

	auto initial = parser->ParseStatement(true);

	auto condition = parser->ParseStatement(true);

	auto increment = parser->parseExpression();

	parser->Consume(TokenType::RightParen);

	auto block = new ScopeExpression(parser->parseBlock());
	return new ForExpression(token, initial, condition, increment, block);
}

Expression* IfParselet::parse(Parser* parser, Token token)
{
	auto branches = new std::vector<Branch*>;
	//todo
	//take parens
	parser->Consume(TokenType::LeftParen);
	Expression* condition = parser->parseExpression();
	parser->Consume(TokenType::RightParen);

	BlockExpression* block = parser->parseBlock(true);

	Branch* branch = new Branch;
	branch->condition = condition;
	branch->block = block;
	branches->push_back(branch);

	Branch* Else = 0;
	while(true)
	{
		//look for elses
		if (parser->MatchAndConsume(TokenType::ElseIf))
		{
			//keep going
			parser->Consume(TokenType::LeftParen);
			Expression* condition = parser->parseExpression();
			parser->Consume(TokenType::RightParen);

			BlockExpression* block = parser->parseBlock(true);

			Branch* branch2 = new Branch;
			branch2->condition = condition;
			branch2->block = block;
			branches->push_back(branch2);
		}
		else if (parser->Match(TokenType::Else))
		{
			//its an else
			parser->Consume();
			BlockExpression* block = parser->parseBlock(true);

			Branch* branch2 = new Branch;
			branch2->condition = 0;
			branch2->block = block;
			Else = branch2;
			break;
		}
		else
			break;//nothing else
	}

	return new IfExpression(token, branches, Else);
}

Expression* FunctionParselet::parse(Parser* parser, Token token)
{
	auto name = new NameExpression(parser->Consume(TokenType::Name).getText());
	auto arguments = new std::vector<Expression*>;

	parser->Consume(TokenType::LeftParen);

	if (!parser->MatchAndConsume(TokenType::RightParen))
	{
		do
		{
			arguments->push_back(parser->ParseStatement(false));
		}
		while( parser->MatchAndConsume(TokenType::Comma));

		parser->Consume(TokenType::RightParen);
	}

	auto block = new ScopeExpression(parser->parseBlock());
	return new FunctionExpression(token, name, arguments, block);
}

Expression* LambdaParselet::parse(Parser* parser, Token token)
{
	parser->Consume(TokenType::LeftParen);

	auto arguments = new std::vector<Expression*>;
	while (true)
	{
		Token name = parser->Consume(TokenType::Name);

		arguments->push_back(new NameExpression(name.getText()));

		if (!parser->MatchAndConsume(TokenType::Comma))
			break;
	}

	parser->Consume(TokenType::RightParen);

	//if (parser->MatchAndConsume(TokenType::
	//if we match shorthand notation, dont parse a scope expression, and just parse the rest of this line
	//leak regarding scope expression somehow
	auto block = new ScopeExpression(parser->parseBlock());
	return new FunctionExpression(token, 0, arguments, block);
}

Expression* CallParselet::parse(Parser* parser, Expression* left, Token token)
{
	auto arguments = new std::vector<Expression*>;

	if (!parser->MatchAndConsume(TokenType::RightParen))
	{
		do
		{
			arguments->push_back(parser->ParseStatement(false));
		}
		while( parser->MatchAndConsume(TokenType::Comma));

		parser->Consume(TokenType::RightParen);
	}

	return new CallExpression(token, left, arguments);
}

Expression* ReturnParselet::parse(Parser* parser, Token token)
{
	Expression* right = 0;
	if (parser->Match(TokenType::Semicolon) == false)
		right = parser->ParseStatement(false);

	return new ReturnExpression(token, right);//CallExpression(token, left, arguments);
}

Expression* LocalParselet::parse(Parser* parser, Token token)
{
	Token name = parser->Consume(TokenType::Name);

	parser->Consume(TokenType::Assign);//its possible this wont be here and it may just be a mentioning, but no assignment

	//todo code me
	Expression* right = parser->parseExpression(/*assignment prcedence -1 */);

	parser->Consume(TokenType::Semicolon);
	//do stuff with this and store and what not
	//need to add this variable to this's block expression

	return new LocalExpression(name, right);
}

Expression* ArrayParselet::parse(Parser* parser, Token token)
{
	parser->Consume(TokenType::RightBracket);
	return new ArrayExpression(token, 0);
}

Expression* IndexParselet::parse(Parser* parser, Expression* left, Token token)
{
	//parser->Consume();
	Expression* index = parser->parseExpression();
	parser->Consume(TokenType::RightBracket);

	return new IndexExpression(left, index);//::atof(token.getText().c_str()));
}

Expression* MemberParselet::parse(Parser* parser, Expression* left, Token token)
{
	//this is for const members
	Expression* member = parser->parseExpression(1);
	NameExpression* name = dynamic_cast<NameExpression*>(member);
	if (name == 0)
		throw ParserException(token.filename, token.line, "Cannot access member name that is not a string");
	auto ret = new IndexExpression(left, new StringExpression(name->GetName()));
	delete name;

	return ret;
}

Expression* ObjectParselet::parse(Parser* parser, Token token)
{
	if (parser->LookAhead().getType() == TokenType::RightBrace)
	{
		parser->Consume();//take the right brace
		//we are done, return null object
		return new ObjectExpression(0);
	}
	//parse initial values
	while(parser->LookAhead().getType() == TokenType::Name)
	{
		Token name = parser->Consume();
		if (parser->MatchAndConsume(TokenType::Comma))
		{
			//keep going
		}
		else
			break;//we are done
	}
	parser->Consume(TokenType::RightBrace);//end part
	return new ObjectExpression(0);
};
