#include <tree_sitter/parser.h>
#include <string>	//string, to_string
#include <stack>	//stack
#include <map>		//map
#include <utility>	//pair
#include <regex>	//regex, regex_match
#include <iostream>

namespace {

/*using std::string;
using std::to_string;
using std::stack;
using std::map;
using std::pair;
using std::regex;
using std::regex_match;
using std::cout;*/
using namespace std;

struct Query;
struct Scanner;

enum TokenType {
	ATTRIBUTE_START,
	ABILITY_START,
	MACRO_START,
	//HTML_ENTITY,
	DICE_ROLL_START,
	TABLE_ROLL_START,
	
	QUERY_START,
	QUERY_PIPE_HASDEFAULT,
	QUERY_PIPE_HASOPTIONS,
	QUERY_END,
	
	
	JUST_AT,
	JUST_PERCENT,
	JUST_HASH,
	JUST_AMPERSAND,
	JUST_D,
	JUST_T,
	JUST_QUESTIONMARK,
	JUST_PIPE,
	//JUST_COMMA,
	//JUST_LEFTBRACE,
	//JUST_RIGHTBRACE,
	//JUST_LEFTBRACKET,
	//JUST_RIGHTBRACKET,
	
	
	_EOF,
	
	_INVALID,
};

enum QueryType {
	QT_INVALID_EMPTY = 1,
	QT_INVALID_UNCLOSED = 2,
	QT_PROMPT_ONLY = 4,
	QT_DEFAULT_VALUE = 8,
	QT_OPTIONS = 16,
};

const bool doLog = true;
void log(string str) { if (doLog) cout << str; }
void logln(string str) { if (doLog) { cout << str; cout << "\n"; } }

char advance(TSLexer *lexer) {
	lexer->advance(lexer, false);
	return lexer->lookahead;
}


bool check_for_closure(TSLexer *lexer, char start_char, char end_char) {
	logln(">>>> check_for_closure");
	char c = lexer->lookahead;
	if (c == start_char) {
		c = advance(lexer);
		while (c != 0 && c != '\n' && c != end_char) {
			c = advance(lexer);
		}
		if (c == end_char) {
			advance(lexer);
			return true;
		}
	}
	return false;
}

bool scan_macro_start(TSLexer *lexer, unsigned depth) {
	logln(">>>> scan_macro_start");
	char c = lexer->lookahead;
	
	if (c == 0 || c == ' ' || c == '\n') return false;
	if (depth > 0 && (c == '|' || c == '}')) return false;
	
	while (c != 0 && c != ' ' && c != '\n') {
		if (depth > 0 && (c == '|' || c == '}')) break;
		if (c == '@' || c == '%') {
			c = advance(lexer);
			if (c == '{' && !check_for_closure(lexer, '{', '}')) return false;
		}
		else {
			c = advance(lexer);
		}
	}
	
	if (c == 0 || c == ' ' || c == '\n') return true;
	
	return false;
}

bool scan_diceRoll_start(TSLexer *lexer) {
	logln(">>>> scan_diceRoll_start");
	char c = lexer->lookahead;
	
	if (c == 'f' || c == 'F') {
		return true;
	}
	else if (regex_match(string({c}), regex("\\d"))) {	//c is a digit
		return true;
	}
	else if (c == '@' || c == '%') {
		advance(lexer);
		if (check_for_closure(lexer, '{', '}')) return true;
	}
	return false;
}

bool scan_tableRoll_start(TSLexer *lexer) {
	logln(">>>> scan_tableRoll_start");
	char c = lexer->lookahead;
	
	bool rightBracketFound = false;
	if (c == '[') {
		c = advance(lexer);
		while (c != 0 && c != '\n' && c != ']') {
			if (c == '@' || c == '%') {
				c = advance(lexer);
				if (c = '{') {
					while (c != 0 && c != '\n' && c != '}' && c != ']') {
						c = advance(lexer);
					}
					if (c == '}') {
						rightBracketFound = false;	//reset this; if it was found, it was just part of the attribute/ability name
						c = advance(lexer);
						continue;
					}
					else if (c == ']') {
						rightBracketFound = true;
					}
					else {	//end of input
						//if rightBracketFound == true, the "@{" or "%{" is interpreted as being part of the table name
						return rightBracketFound;
					}
				}
				else {
					continue;
				}
			}
			c = advance(lexer);
		}
		if (c == ']') return true;
	}
	return false;
}


map<string, string> entitiesMap;

void populateEntitiesMap() {
	if (!entitiesMap.empty()) return;
	entitiesMap.insert(pair<string, string>("@", "#64|#[xX](00)?40|commat"));
	entitiesMap.insert(pair<string, string>("%", "#37|#[xX](00)?25|percnt"));
	entitiesMap.insert(pair<string, string>("#", "#35|#[xX](00)?23|num"));
	entitiesMap.insert(pair<string, string>("&", "#38|#[xX](00)?26|amp|AMP"));
	entitiesMap.insert(pair<string, string>("[", "#91|#[xX](00)?5[bB]|lsqb|lbrack"));
	entitiesMap.insert(pair<string, string>("(", "#40|#[xX](00)?28|lpar"));
	entitiesMap.insert(pair<string, string>("{", "#123|#[xX](00)?7[bB]|lcub|lbrace"));
	entitiesMap.insert(pair<string, string>("+", "#43|#[xX](00)?2[bB]|plus"));
	entitiesMap.insert(pair<string, string>("-", "#45|#[xX](00)?2[dD]|dash|hyphen"));
	entitiesMap.insert(pair<string, string>("}", "#125|#[xX](00)?7[dD]|rcub|rbrace"));
	entitiesMap.insert(pair<string, string>(")", "#41|#[xX](00)?29|rpar"));
	entitiesMap.insert(pair<string, string>("]", "#93|#[xX](00)?5[dD]|rsqb|rbrack"));
	entitiesMap.insert(pair<string, string>("|", "#123|#[xX](00)?7[cC]|vert|verbar|VerticalLine"));
	entitiesMap.insert(pair<string, string>("?", "#63|#[xX](00)?3[fF]|quest"));
	entitiesMap.insert(pair<string, string>(",", "#44|#[xX](00)?2[cC]|comma"));
	entitiesMap.insert(pair<string, string>("=", "#61|#[xX](00)?3[dD]|equals"));
	entitiesMap.insert(pair<string, string>("~", "#126|#[xX](00)?7[eE]"));
	entitiesMap.insert(pair<string, string>("d", "#100|#[xX](00)?64"));
	entitiesMap.insert(pair<string, string>("D", "#68|#[xX](00)?44"));
	entitiesMap.insert(pair<string, string>("t", "#116|#[xX](00)?74"));
	entitiesMap.insert(pair<string, string>("T", "#84|#[xX](00)?54"));
}

bool matchDelimiter(string entity, string delimiter, unsigned depth, bool shallowerIsOkay = false) {
	if (depth == 0) return entity.compare(delimiter) == 0;
	if (shallowerIsOkay && entity.compare(delimiter) == 0) return true;
	string rxpStr = "&(amp;){";
	rxpStr.append(shallowerIsOkay?"0,":"");
	rxpStr.append(to_string(depth-1));
	rxpStr.append("}(");
	rxpStr.append(entitiesMap[delimiter]);
	rxpStr.append(")");
	return regex_match(entity, regex(rxpStr));
}

string get_entity(TSLexer *lexer, unsigned depth, bool shallowerIsOkay = false) {
	logln(">>>> get_entity");
	regex charsRxp = regex("[a-zA-Z0-9]");
	string code = "";
	string entity = "&";
	unsigned amps = 0;
	
	char c = lexer->lookahead;
	
	if (c != '&') {
		logln("->>> not an ampersand '"+string({c})+"'");
		return "";
	}
	c = advance(lexer);
	
	while (c != 0){
		if (c == '#' && code == "") {
			entity += "#";
			c = advance(lexer);
			if (c == 'x' || c == 'X') {
				entity += string({c});
				charsRxp = regex("[a-fA-F0-9]");
				c = advance(lexer);
			}
			else {
				charsRxp = regex("\\d");
			}
		}
		else {
			charsRxp = regex("[a-zA-Z0-9]");
		}
		
		if (c == ';') {
			entity += code + ";";
			if (depth == 0 || amps == depth-1) {
				advance(lexer);
				if (code.length() < 2){
					logln("->>> entity name too short: \""+code+"\"");
					return "";
				}
				logln("->>> found \""+entity+"\"");
				return entity;
			}
			else if (code.compare("amp") == 0) {
				amps++;
				code = "";
			}
			else if (shallowerIsOkay) {
				advance(lexer);
				logln("->>> found \""+entity+"\"");
				return entity;
			}
			else{
				advance(lexer);
				logln("->>> depth != "+to_string(depth)+" for \""+entity+"\"");
				return "";
			}
		}
		else if (!regex_match(string({c}), charsRxp)){
			c = advance(lexer);
			logln("->>> invalid character '"+string({c})+"'");
			return "";
		}
		code += c;
		c = advance(lexer);
	}
	logln("->>> eof");
	return "";
}
bool get_entity_delimiter(TSLexer *lexer, string toMatch, unsigned depth, bool shallowerIsOkay = false) {
	logln(">>>> get_entity_delimiter");
	string entity = get_entity(lexer, depth, shallowerIsOkay);
	if (entity == "") return "";
	return matchDelimiter(entity, toMatch, depth, shallowerIsOkay);
}
bool get_entity_value(TSLexer *lexer, unsigned depth) {
	logln(">>>> get_entity_value");
	return get_entity(lexer, depth+1, false) != "";
}


struct Query {
	Scanner *scanner;
	unsigned depth = 0;
	enum QueryPart : unsigned { qpStart, qpPrompt, qpDefault, qpName, qpValue };
	int type = 0;
	
	Query(Scanner *scanner, unsigned depth) : scanner(scanner), depth(depth) {}
	
	/*string serialize() {
		logln(">>>> Query.serialize");
		log("->>> Query serialized to \"" + to_string(depth)+" "+to_string(type)+" "+to_string(optionCount) + " \"\n");
		return to_string(depth)+" "+to_string(type)+" "+to_string(optionCount)+" ";
	}*/
	
	/*void deserialize(const char *buffer) {
		string data = string(buffer);
		logln(">>>> Query.deserialize \""+data+"\"");
		size_t p;
		depth = stoul(data, &p);
		type = stoi(data.substr(p+1), &p);
		optionCount = stoul(data.substr(p+1), &p);
	}*/
	
	bool scan_query_start(TSLexer *lexer) {
		logln(">>>> Query.scan_query_start");
		char c = lexer->lookahead;
		
		if (c == '{') {
			advance(lexer);
			return true;
		}
		else if (c == '&') {
			return get_entity_delimiter(lexer, "{", depth, true);
		}
		else {
			return false;
		}
	}
	
	bool scan_query_prompt(TSLexer *lexer, int *type) {
		logln(">>>> Query.scan_query_prompt");
		char c = lexer->lookahead;
		bool hasLength = false;
		string entity = "";
		
		while (c != 0) {
			if (c == '}') {
				if (!hasLength) {
					*type = this->type |= QT_INVALID_EMPTY;
				}
				else {
					*type = this->type |= QT_PROMPT_ONLY;
				}
				advance(lexer);
				return hasLength;
			}
			if (c == '|') {
				advance(lexer);
				return true;
			}
			if (c == '&') {
				entity = get_entity(lexer, depth, false);
				if (entity != "") {
					if (matchDelimiter(entity, "}", depth, false)){
						if (!hasLength) {
							*type = this->type |= QT_INVALID_EMPTY;
						}
						return hasLength;
					}
					if (matchDelimiter(entity, "|", depth, false)){
						return true;
					}
				}
			}
			hasLength = true;
			c = advance(lexer);
		}
		
		*type = this->type |= QT_INVALID_UNCLOSED;
		return false;
	}
	
	bool scan_query_values(TSLexer *lexer, int *type) {
		logln(">>>> Query.scan_query_values");
		char c = lexer->lookahead;
		string entity = "";
		
		while (c != 0) {
			if ((c == '}' || c == '|') && depth > 0) {
				*type = this->type |= QT_INVALID_UNCLOSED;
				return false;
			}
			if (c == '}'){
				if (!(this->type & QT_OPTIONS)) *type = this->type |= QT_DEFAULT_VALUE;
				return true;
			}
			if (c == '|') {
				*type = this->type |= QT_OPTIONS;
			}
			if (c == ',' && this->type & QT_OPTIONS) {
			}
			else if (c == '&') {
				entity = get_entity(lexer, depth, false);
				if (entity != "") {
					if (matchDelimiter(entity, "}", depth, false)){
						if (!(*type & QT_OPTIONS)) *type = this->type |= QT_DEFAULT_VALUE;
						return true;
					}
					else if (matchDelimiter(entity, "|", depth, false)){
						*type = this->type |= QT_OPTIONS;
					}
				}
			}
			c = advance(lexer);
		}
		
		*type = this->type |= QT_INVALID_UNCLOSED;
		return false;
	}
	
};

struct Scanner {
	stack<Query*> queries;
	
	Scanner() {
		populateEntitiesMap();
	}
	
	unsigned serialize(char *buffer) {
		return 0;
		/*logln(">>>> Scanner.serialize");
		string data = "";
		while(queries.size() > 0) {
			data += queries.top()->serialize();
			queries.pop();
			logln("-->> pop a Query: "+to_string(queries.size()));
		}
		logln("->>> Scanner serialized to \"" + data + "\"");
		strcpy(buffer, data.c_str());	//convert from string to char*
		if (strlen(buffer) >= TREE_SITTER_SERIALIZATION_BUFFER_SIZE) return 0;	//TODO: is there a better option?
		return strlen(buffer);*/
	}

	void deserialize(const char *buffer, unsigned length) {
		/*while (queries.size() > 0) {
			queries.pop();
		}
		
		if (length == 0) {
			//
			logln(">>>> Scanner.deserialize \"\"");
		} else {
			logln(">>>> Scanner.deserialize \"" + string(buffer) + "\"");
			string data = string(buffer)+" ";
			size_t p;
			do {
				p = data.find(" ");
				p = data.find(" ", p+1);
				p = data.find(" ", p+1);
				if (p == string::npos) break;
				Query *query = new Query(this, queries.size());
				
				//for some reason, I can't just do
				// `const char *temp = data.substr(0,p).c_str();`
				// or `const char *temp = (data.substr(0,p)).c_str();`
				string queryData = data.substr(0,p+1);
				const char *temp = queryData.c_str();
				
				query->deserialize(temp);
				queries.push(query);
				logln("->>> push a Query: "+to_string(queries.size()));
				data = data.substr(p+1);
			} while (data.length() > 1);
		}*/
	}
	
	void consume_query() {
		//TODO: push a new Query to the stack and scan for the end of it (just to skip it), then pop it.
	}
	
	bool scan(TSLexer *lexer, const bool *valid_symbols){
		logln(">>>> Scanner.scan");
/*if(valid_symbols[_EOF])log("_EOF,");
if(valid_symbols[JUST_AT])log("JUST_AT,");
if(valid_symbols[ATTRIBUTE_START])log("ATTRIBUTE_START,");
if(valid_symbols[JUST_PERCENT])log("JUST_PERCENT,");
if(valid_symbols[ABILITY_START])log("ABILITY_START,");
if(valid_symbols[JUST_HASH])log("JUST_HASH,");
if(valid_symbols[MACRO_START])log("MACRO_START,");
if(valid_symbols[JUST_D])log("JUST_D,");
if(valid_symbols[DICE_ROLL_START])log("DICE_ROLL_START,");
if(valid_symbols[JUST_T])log("JUST_T,");
if(valid_symbols[TABLE_ROLL_START])log("TABLE_ROLL_START,");
if(valid_symbols[JUST_QUESTIONMARK])log("JUST_QUESTIONMARK,");
if(valid_symbols[QUERY_START])log("QUERY_START,");
if(valid_symbols[QUERY_PIPE_HASDEFAULT])log("QUERY_PIPE_HASDEFAULT,");
if(valid_symbols[QUERY_PIPE_HASOPTIONS])log("QUERY_PIPE_HASOPTIONS,");
logln("]");*/
		lexer->mark_end(lexer);
		logln("----------");
		char c = lexer->lookahead;
		int queryType = 0;
		
		logln("->>> start character: '"+string({c})+"'");
		
		if (c == 0) {
			logln("->>> _EOF");
			lexer->result_symbol = _EOF;
			return valid_symbols[_EOF];
		}
		else if (c == '@') {
			logln("->>> '@'");
			if (valid_symbols[ATTRIBUTE_START] || valid_symbols[JUST_AT] || valid_symbols[_INVALID]) {
				c = advance(lexer);
				lexer->mark_end(lexer);
				logln("----------");
				
				if (valid_symbols[ATTRIBUTE_START] && check_for_closure(lexer, '{', '}')) {
					logln("-->> ATTRIBUTE_START");
					lexer->result_symbol = ATTRIBUTE_START;
					return true;
				}
				else if (valid_symbols[_INVALID] && c == '{') {
					lexer->mark_end(lexer);
					logln("-->> _INVALID");
					lexer->result_symbol = _INVALID;
					return true;
				}
				else if (valid_symbols[JUST_AT]) {
					logln("-->> JUST_AT");
					lexer->result_symbol = JUST_AT;
					return true;
				}
			}
		}
		else if (c == '%') {
			logln("->>> '%'");
			if (valid_symbols[ABILITY_START] || valid_symbols[JUST_PERCENT] || valid_symbols[_INVALID]) {
				c = advance(lexer);
				lexer->mark_end(lexer);
				logln("----------");
				
				if (valid_symbols[ABILITY_START] && check_for_closure(lexer, '{', '}')) {
					logln("-->> ABILITY_START");
					lexer->result_symbol = ABILITY_START;
					return true;
				}
				else if (valid_symbols[_INVALID] && c == '{') {
					lexer->mark_end(lexer);
					logln("-->> _INVALID");
					lexer->result_symbol = _INVALID;
					return true;
				}
				else if (valid_symbols[JUST_PERCENT]) {
					logln("-->> JUST_PERCENT");
					lexer->result_symbol = JUST_PERCENT;
					return true;
				}
			}
		}
		else if (c == '#') {
			logln("->>> '#'");
			if (valid_symbols[MACRO_START] || valid_symbols[JUST_HASH]) {
				advance(lexer);
				lexer->mark_end(lexer);
				logln("----------");
				
				if (valid_symbols[MACRO_START] && scan_macro_start(lexer, queries.size())) {
					logln("-->> MACRO_START");
					lexer->result_symbol = MACRO_START;
					return true;
				}
				else if (valid_symbols[JUST_HASH]) {
					logln("-->> JUST_HASH");
					lexer->result_symbol = JUST_HASH;
					return true;
				}
			}
		}
		else if (c == '&') {
			logln("->>> '&'");
			
			if (valid_symbols[QUERY_START]) {
				if (get_entity_delimiter(lexer, "?", queries.size(), true)) {
					logln("-->> '?'");
					lexer->mark_end(lexer);
					logln("----------");
					
					Query *query = new Query(this, queries.size());
					queries.push(query);
					logln("-->> push a Query: "+to_string(queries.size()));
					
					if (queries.top()->scan_query_start(lexer)) {
						lexer->mark_end(lexer);
						logln("----------");
						
						if (queries.top()->scan_query_prompt(lexer, &queryType)) {
							if (queries.top()->scan_query_values(lexer, &queryType)) {
								logln("---> QUERY_START");
								lexer->result_symbol = QUERY_START;
								return true;
							}
						}
					}
					
					queries.pop();
					logln("-->> pop a Query: "+to_string(queries.size()));
					return false;
				}
			}
			else if ((valid_symbols[QUERY_PIPE_HASDEFAULT] || valid_symbols[QUERY_PIPE_HASOPTIONS]) && queries.size() > 1) {
				if (get_entity_delimiter(lexer, "|", queries.size(), false)) {
					logln("-->> '|'");
					lexer->mark_end(lexer);
					logln("----------");
					
					if (queries.top()->scan_query_values(lexer, &queryType)) {
						if (queryType & QT_DEFAULT_VALUE) {
							logln("---> QUERY_PIPE_HASDEFAULT");
							lexer->result_symbol = QUERY_PIPE_HASDEFAULT;
						}
						else if (queryType & QT_OPTIONS) {
							logln("---> QUERY_PIPE_HASOPTIONS");
							lexer->result_symbol = QUERY_PIPE_HASOPTIONS;
						}
						return true;
					}
				}
			}
			else if (valid_symbols[QUERY_END] && queries.size() == 1) {
				if (get_entity_delimiter(lexer, "}", queries.size(), false)) {
					logln("-->> '}'");
					lexer->mark_end(lexer);
					logln("----------");
					
					logln("---> QUERY_END");
					lexer->result_symbol = QUERY_END;
					queries.pop();
					logln("-->> pop a Query: "+to_string(queries.size()));
					return true;
				}
			}
			else if (valid_symbols[JUST_PIPE] && queries.size() > 0) {
				if (get_entity_delimiter(lexer, "|", queries.size(), true)) {
					logln("-->> '|'");
					lexer->mark_end(lexer);
					logln("----------");
					
					logln("---> JUST_PIPE");
					lexer->result_symbol = JUST_PIPE;
					return true;
				}
			}
			else {
				advance(lexer);
				lexer->mark_end(lexer);
				logln("----------");
				
				logln("---> JUST_AMPERSAND");
				lexer->result_symbol = JUST_AMPERSAND;
				return true;
			}
		}
		else if (c == 'd' || c == 'D') {
			log("->>> '"+string({c})+"'\n");
			if (valid_symbols[DICE_ROLL_START] || valid_symbols[JUST_D]) {
				c = advance(lexer);
				lexer->mark_end(lexer);
				logln("----------");
				
				if (valid_symbols[DICE_ROLL_START] && scan_diceRoll_start(lexer)) {
					logln("-->> DICE_ROLL_START");
					lexer->result_symbol = DICE_ROLL_START;
					return true;
				}
				
				if (valid_symbols[JUST_D]) {
					logln("-->> JUST_D");
					lexer->result_symbol = JUST_D;
					return true;
				}
			}
		}
		else if (c == 't' || c == 'T') {
			log("->>> '"+string({c})+"'\n");
			if (valid_symbols[TABLE_ROLL_START] || valid_symbols[JUST_T]) {
				c = advance(lexer);
				lexer->mark_end(lexer);
				logln("----------");
				
				if (valid_symbols[TABLE_ROLL_START] && scan_tableRoll_start(lexer)) {
					logln("-->> TABLE_ROLL_START");
					lexer->result_symbol = TABLE_ROLL_START;
					return true;
				}
				else if (valid_symbols[JUST_T]) {
					logln("-->> JUST_T");
					lexer->result_symbol = JUST_T;
					return true;
				}
			}
		}
		else if (c == '?') {
			logln("->>> '?'");
			if (valid_symbols[QUERY_START] || valid_symbols[JUST_QUESTIONMARK]) {
				c = advance(lexer);
				lexer->mark_end(lexer);
				logln("----------");
				
				if (valid_symbols[QUERY_START]) {
					Query *query = new Query(this, queries.size());
					queries.push(query);
					logln("-->> push a Query: "+to_string(queries.size()));
					
					if (queries.top()->scan_query_start(lexer)) {
						lexer->mark_end(lexer);
						logln("----------");
						
						if (queries.top()->scan_query_prompt(lexer, &queryType)) {
							if (queries.top()->scan_query_values(lexer, &queryType)) {
								logln("---> QUERY_START");
								lexer->result_symbol = QUERY_START;
								return true;
							}
						}
					}
					
					queries.pop();
					logln("-->> pop a Query: "+to_string(queries.size()));
					return false;
				}
				
				if (valid_symbols[JUST_QUESTIONMARK]) {
					logln("-->> JUST_QUESTIONMARK");
					lexer->result_symbol = JUST_QUESTIONMARK;
					return true;
				}
			}
		}
		else if (c == '|') {
			logln("->>> '|'");
			if ((valid_symbols[QUERY_PIPE_HASDEFAULT] || valid_symbols[QUERY_PIPE_HASOPTIONS]) && queries.size() == 1) {
				advance(lexer);
				lexer->mark_end(lexer);
				logln("----------");
				
				logln("-->> get first pipe");
				if (queries.top()->scan_query_values(lexer, &queryType)) {
					if (queryType & QT_DEFAULT_VALUE) {
						logln("---> QUERY_PIPE_HASDEFAULT");
						lexer->result_symbol = QUERY_PIPE_HASDEFAULT;
					}
					else if (queryType & QT_OPTIONS) {
						logln("---> QUERY_PIPE_HASOPTIONS");
						lexer->result_symbol = QUERY_PIPE_HASOPTIONS;
					}
					return true;
				}
			}
			else if (valid_symbols[JUST_PIPE]) {
				advance(lexer);
				lexer->mark_end(lexer);
				logln("----------");
				
				logln("---> JUST_PIPE");
				lexer->result_symbol = JUST_PIPE;
				return true;
			}
		}
		else if (c == '}') {
			logln("->>> '}'");
			if (valid_symbols[QUERY_END] && queries.size() == 1) {
				c = advance(lexer);
				lexer->mark_end(lexer);
				logln("----------");
				
				logln("---> QUERY_END");
				lexer->result_symbol = QUERY_END;
				queries.pop();
				logln("-->> pop a Query: "+to_string(queries.size()));
				return true;
			}
		}
		else {
			log("->>> (no match)\n");
			return false;
		}
		return false;
	}
};

}

extern "C" {

void *tree_sitter_roll20_script_external_scanner_create() {
	return new Scanner();
}

bool tree_sitter_roll20_script_external_scanner_scan(void *payload, TSLexer *lexer, const bool *valid_symbols) {
	Scanner *scanner = static_cast<Scanner *>(payload);
	return scanner->scan(lexer, valid_symbols);
}

unsigned tree_sitter_roll20_script_external_scanner_serialize(void *payload, char *state) {
	Scanner *scanner = static_cast<Scanner *>(payload);
	return scanner->serialize(state);
}

void tree_sitter_roll20_script_external_scanner_deserialize(void *payload, const char *state, unsigned length) {
	Scanner *scanner = static_cast<Scanner *>(payload);
	scanner->deserialize(state, length);
}

void tree_sitter_roll20_script_external_scanner_destroy(void *payload) {
	Scanner *scanner = static_cast<Scanner *>(payload);
	delete scanner;
}

}
