#pragma once
#include <string>
#include "PatternClause.h"
#include "AssgNode.h"

using namespace std;

class PatternAssgClause :
	public PatternClause {

public:
	PatternAssgClause(const string& syn);
	PatternAssgClause(const string& syn, const string& var, const string& expr);
	~PatternAssgClause(void);
	
	string getExpression();
	Results evaluate(Results*);
	bool isValid();

	void setExpression(string expr);

private:
	Results* evaluateVarWildExprWild(vector<int>& assgNums, Results*);
	Results evaulateVarWildExpr(vector<int>& assgNums, string expr, Results);
	Results evaluateVarFixedExprWild(vector<int>& assgNums, Results);
	Results evaluateVarFixedExpr(vector<int>& assgNums, string expr, Results);
	Results evaluateVarExprWild(vector<int>& assgNums, vector<string>& varNames, Results);
	Results evaluateVarExpr(vector<int>& assgNums, vector<string>& varNames, string expr, Results);

	vector<int> getAssgNums(Results, string);
	bool matchExpr(AssgNode* assg, string expr);
	bool matchVar(AssgNode* assg, string var);
	
	string _expr;
};

