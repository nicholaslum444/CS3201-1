#include "PatternAssgClause.h"
#include "Utils.h"
#include "StmtTable.h"
#include "VarTable.h"
#include "OpNode.h"

#include <set>
#include <stack>
#include <iostream>
#include "boost\lexical_cast.hpp"
#include "boost\algorithm\string.hpp"
#include "boost\unordered_map.hpp"
#include "boost\unordered\unordered_map.hpp"
#include "boost\foreach.hpp"

using namespace stringconst;
using namespace boost;

PatternAssgClause::PatternAssgClause(const string& syn) 
	: PatternClause() {
	/*firstArgType = ARG_ASSIGN;
	firstArg = syn;
	firstArgFixed = false;
	secondArgType = ARG_GENERIC;
	secondArgFixed = false;*/
} 

PatternAssgClause::PatternAssgClause(const string& syn, const string& var, const string& expr) 
	: PatternClause() {
	synType = ARG_ASSIGN;
	this->syn = syn;
	this->var = var;
	// parser must set vartype and varfixed.
	this->_expr = expr;
}

PatternAssgClause::~PatternAssgClause(void) {
}

string PatternAssgClause::getExpression() {
	return _expr;
}

bool PatternAssgClause::isExprWild() {
	return getExpression() == STRING_EMPTY;
}

bool PatternAssgClause::isValid() {
	string varType = this->getVarType();
	bool checkVar = (varType == ARG_VARIABLE) || (varType == ARG_GENERIC);
	bool valid = checkVar;
	return valid;
}

bool PatternAssgClause::matchPattern(string stmtNumStr, string varName) {
	int stmtNum = stoi(stmtNumStr);
	AssgNode* assgNode = (AssgNode*) this->stmtTable->getStmtObj(stmtNum)->getTNodeRef();
	return matchVar(assgNode, varName) && matchExpr(assgNode, getExpression());
}

unordered_set<string> PatternAssgClause::getAllSynValues() {
	unordered_set<Statement*> allStmtObjs = stmtTable->getAssgStmts();
	unordered_set<string> allSynValues;
	BOOST_FOREACH(Statement* stmtObj, allStmtObjs) {
		allSynValues.insert(lexical_cast<string>(stmtObj->getStmtNum()));
	}
	return allSynValues;
}

bool PatternAssgClause::matchVar(AssgNode* assgnode, string var) {

	// match var based on varnode name
	VarNode* varnode = assgnode->getVarNode();

	return varnode->getName() == var;
}

bool PatternAssgClause::matchExpr(AssgNode* assg, string expr) {

	// match expr based on the assgnode
	if (assg == NULL) {
		return false;
	}

	if (isExprWild()) {
		return true;
	}

	// TODO
	// account for both _"x+y"_ and "x+y"
	// so far only got "x+y"

	int i = 0;

	// stuff the rpn into the stack
	string rpn = expr.substr(2, expr.length() - 4);
	//std::stack<string> tempstack = stack<string>();
	//std::stack<string> rpnstack = stack<string>();
	std::vector<string> rpnarr = vector<string>();
	boost::split(rpnarr, rpn, boost::is_any_of(" "));


	// this is buggy. it breaks a many-letter var into all its letters.
	// use string split instead
	//for (size_t i = 0; i < rpn.length(); i++) {
	//	string token = rpn.substr(i, 1);
	//	if (token != " ") {
	//		//tempstack.push(token);
	//		rpnarr.push_back(token);
	//	}
	//}

	// do dfs
	set<TNode*> visited;
	stack<TNode*> nodestack = stack<TNode*>();
	TNode* exprnode = assg->getExprNode();
	//cout << exprnode->getName() << endl;
	nodestack.push(exprnode);
	TNode* nextnode = nodestack.top();
	while (nextnode != NULL && i < 9999) {
		i++;
		if (nextnode->getNodeType() == OPERATOR_) {
			OpNode* op = (OpNode*) nextnode;
			nodestack.push(op->getRightNode());
			nodestack.push(op->getLeftNode());
			nextnode = nodestack.top();
			//cout << nextnode->getName() << endl;
		} else if (nextnode->getNodeType() == CONSTANT_) {
			nextnode = NULL;
		} else if (nextnode->getNodeType() == VARIABLE_) {
			nextnode = NULL;
		} else {
			return false;
		}
	}

	// compare rpn with dfs nodes
	// once start to match rpn must match all the way
	// then only considered subtree
	for (size_t compared = 0; compared < rpnarr.size() && i < 9999; compared++) {
		if (nodestack.empty()) {
			return false;
		}
		TNode* node = nodestack.top();
		string token = rpnarr.at(compared);
		//cout << "conparing" << endl;
		//cout << node->getName() << endl;
		//cout << token << endl;
		if (node->getName() == token) {
			nodestack.pop();
			// if we break a streak of matches,
			// then it's not subtree
		} else {
			// start from the front of the rpn again
			// -1 cos it will ++ on the iteration
			compared = -1;
			nodestack.pop();
		}
	}

	return true;
}