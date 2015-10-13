/*
 * CFGBuilder.cpp
 *
 *  Created on: 14 Sep, 2015
 *      Author: Leon
 */

#include "CFGBuilder.h"

using namespace std;

bool CFGBuilder::instanceFlag = false;
CFGBuilder* CFGBuilder::cfgBuilderInstance = NULL;

CFGBuilder::CFGBuilder() {
	stmtCounter = 0;
	currNestingLevel = 0;
	currentProcedure = NULL;
	head = NULL;
	prevState = NONE;
}

CFGBuilder* CFGBuilder::getInstance() {
	if(!instanceFlag) {
		cfgBuilderInstance = new CFGBuilder();
		instanceFlag = true;
	}

	return cfgBuilderInstance;
}

void CFGBuilder::resetInstanceFlag() {
	instanceFlag = false;
}

void CFGBuilder::processParsedData(ParsedData data) {
	switch(data.getType()) {
		case ParsedData::PROCEDURE:
			processProcStmt(data);
			break;
		case ParsedData::ASSIGNMENT:
			processAssgStmt(data);
			break;
		case ParsedData::CALL:
			processCallStmt(data);
			break;
		case ParsedData::IF:
			processIfStmt(data);
			break;
		case ParsedData::ELSE:
			processElseStmt(data);
			break;
		case ParsedData::WHILE:
			processWhileStmt(data);
			break;
		case ParsedData::END:
			processEndProgram(data);
			break;
		default:
			break;
	}
}

void CFGBuilder::processProcStmt(ParsedData data) {
	CFG* cfg = CFG::getInstance();

	ProcGNode* procNode = new ProcGNode(data.getProcName());
	EndGNode* endNode = new EndGNode();

	int diff = currNestingLevel - 1;
	for(int i = diff; i > 0; i--) {
		if(i > 1) {
			if(nestingStack.top()->getNodeType() == IF_) {
				setDummyIfNestingRecovery();
			} else {
				setWhileNestingRecovery();

				head = nestingStack.top();
				nestingStack.pop();
			}
		} else /* i == 1 */ {
			if(nestingStack.top()->getNodeType() == IF_) {
				setEndProcOrEndNodeIfNest(endNode);
			} else {
				setWhileNestingRecovery();

				nestingStack.top()->setSecondChild(endNode);
				endNode->setFirstParent(nestingStack.top());
			}

			nestingStack.pop();
		}
	}

	if(diff == 0) {
		head->setFirstChild(endNode);
		endNode->setFirstParent(head);
	}

	if(currentProcedure != NULL) {
		cfg->addProcedure(currentProcedure);
	}

	head = procNode;
	currentProcedure = procNode;
	setProcReference(procNode);
	currNestingLevel = data.getNestingLevel() + 1;
	prevState = PROCEDURE;
}

void CFGBuilder::processAssgStmt(ParsedData data) {
	// Assg and calls can have 3 scenarios of nesting level
	// Either the nesting increases by 1, or it recovers by any amount
	// or stays the same
	if(data.getNestingLevel() < currNestingLevel) {
		int diff = currNestingLevel - data.getNestingLevel();
		AssgGNode* node = new AssgGNode(++stmtCounter);
		setStatementReference(node);
		for(int i = diff; i > 0; i--) {
			if(i > 1) {
				if(nestingStack.top()->getNodeType() == IF_) {
					setDummyIfNestingRecovery();
				} else {
					setWhileNestingRecovery();

					head = nestingStack.top();
					nestingStack.pop();
				}
			} else /* i == 1 */ {
				if(nestingStack.top()->getNodeType() == IF_) {
					setNodeSameRecoveryIfNest(node);
				} else /* top is while */ {
					setWhileNestingRecovery();

					// add the assignment after all the recovery
					nestingStack.top()->setSecondChild(node);
					node->setFirstParent(nestingStack.top());
					addNextAndPrev(node, nestingStack.top());

					head = node;
					nestingStack.pop();
				}
			}
		}
	} else /* entering nesting */ {
		if(prevState == ASSG) {
			int prevStmt = head->getEndStmt();
			++stmtCounter;

			setStatementReference(head);
			addNextAndPrev(stmtCounter, prevStmt);
			head->setEndStmt(stmtCounter);
			return;
		}

		AssgGNode* node = new AssgGNode(++stmtCounter);
		checkPrevStateAndSetProperties(node);
	}

	prevState = ASSG;
	currNestingLevel = data.getNestingLevel();
}

void CFGBuilder::processIfStmt(ParsedData data) {
	IfGNode* node = new IfGNode(++stmtCounter);
	setStatementReference(node);

	if(currNestingLevel > data.getNestingLevel()) {
		int diff = currNestingLevel - data.getNestingLevel();

		for(int i = diff; i > 0; i--) {
			if (i > 1) {
				if (nestingStack.top()->getNodeType() == IF_) {
					setDummyIfNestingRecovery();
				} else {
					setWhileNestingRecovery();

					head = nestingStack.top();
					nestingStack.pop();
				}
			} else /* i == 1 */{
				if(nestingStack.top()->getNodeType() == IF_) {
					setNodeSameRecoveryIfNest(node);
				} else /* while stmt */{
					setWhileNestingRecovery();

					//recovery
					nestingStack.top()->setSecondChild(node);
					node->setFirstParent(nestingStack.top());
					addNextAndPrev(node, nestingStack.top());

					head = node;
					nestingStack.pop();
				}
			}
		}

	} else {
		// if prev is assg/call it can only continue on with the same nesting level
		if(prevState == ASSG) {
			prevState = IF;

			int prevInt = head->getEndStmt();
			addNextAndPrev(stmtCounter, prevInt);

			head->setFirstChild(node);
			node->setFirstParent(head);
			head = node;

			nestingStack.push(node);

			return;
		}

		// prev state was either if/else/while
		// will definitely enter nesting
		checkPrevStateAndSetProperties(node);
	}

	currNestingLevel = data.getNestingLevel() + 1;
	prevState = IF;
	nestingStack.push(node);
}

void CFGBuilder::processElseStmt(ParsedData data) {
	// dummy node parents will always set the dummy node as the first child
	// when processing an else stmt, the nesting level will always decrease
	// when nesting level decreases by one, the top of the nesting stack will
	// always be an if to pair it up
	int diff = currNestingLevel - data.getNestingLevel();

	for(int i = diff; i > 0; i--) {
		if(i == 1) {
			prevState = ELSE;
			ifStack.push(head);
			head = nestingStack.top();
		} else {
			if(nestingStack.top()->getNodeType() == IF_) {		// recovering from an if nest
				setDummyIfNestingRecovery();
			} else /* nesting stack top == while */{											// recovering from a while nest
				GNode* whileParent = nestingStack.top();
				head->setFirstChild(whileParent);
				setRecoverParent(whileParent, head);
				whileParent->setSecondParent(head);
				addNextAndPrev(whileParent, head);

				head = whileParent;
				nestingStack.pop();
			}
		}
	}

	currNestingLevel = data.getNestingLevel() + 1;
}

void CFGBuilder::processWhileStmt(ParsedData data) {
	WhileGNode* node = new WhileGNode(++stmtCounter);
	setStatementReference(node);

	if(currNestingLevel > data.getNestingLevel()) {
		int diff = currNestingLevel - data.getNestingLevel();

		for(int i = diff; i > 0; i--) {
			if (i > 1) {
				if (nestingStack.top()->getNodeType() == IF_) {
					setDummyIfNestingRecovery();
				} else {
					setWhileNestingRecovery();

					head = nestingStack.top();
					nestingStack.pop();
				}
			} else /* i == 1 */{
				if(nestingStack.top()->getNodeType() == IF_) {
					setNodeSameRecoveryIfNest(node);
				} else /* while stmt */{
					setWhileNestingRecovery();

					//recovery
					nestingStack.top()->setSecondChild(node);
					node->setFirstParent(nestingStack.top());
					addNextAndPrev(node, nestingStack.top());

					head = node;
					nestingStack.pop();
				}
			}
		}
	} else /* entering nesting or stay the same */ {
		if(prevState == ASSG) { /* same nesting level */
			prevState = WHILE;

			addNextAndPrev(node, head);

			head->setFirstChild(node);
			node->setFirstParent(head);
			head = node;

			nestingStack.push(node);

			return;
		}

		checkPrevStateAndSetProperties(node);
	}

	currNestingLevel = data.getNestingLevel() + 1;
	prevState = WHILE;
	nestingStack.push(node);
}

void CFGBuilder::processCallStmt(ParsedData data) {
	CallGNode* node = new CallGNode(++stmtCounter);
	setStatementReference(node);

	if(currNestingLevel > data.getNestingLevel()) {
		int diff = currNestingLevel - data.getNestingLevel();

		for(int i = diff; i > 0; i--) {
			if(i > 1) {
				if(nestingStack.top()->getNodeType() == IF_) {
					setDummyIfNestingRecovery();
				} else {
					setWhileNestingRecovery();

					head = nestingStack.top();
					nestingStack.pop();
				}
			} else /* i == 1 */ {
				if(nestingStack.top()->getNodeType() == IF_) {
					setNodeSameRecoveryIfNest(node);
				} else {
					setWhileNestingRecovery();

					// recovery
					nestingStack.top()->setSecondChild(node);
					node->setFirstParent(nestingStack.top());
					addNextAndPrev(node, nestingStack.top());

					head = node;
					nestingStack.pop();
				}
			}
		}
	} else /* entering nesting */{
		if(prevState == ASSG) {
			prevState = CALL;
			
			int prevInt = head->getEndStmt();
			addNextAndPrev(stmtCounter, prevInt);
			
			head->setFirstChild(node);
			node->setFirstParent(head);
			head = node;

			nestingStack.push(node);

			return;
		}

		checkPrevStateAndSetProperties(node);
	}

	currNestingLevel = data.getNestingLevel();
	prevState = CALL;
}

void CFGBuilder::processEndProgram(ParsedData data) {
	CFG* cfg = CFG::getInstance();

	EndGNode* endNode = new EndGNode();

	int diff = currNestingLevel - 1;

	for(int i = diff; i > 0; i--) {
		if(i > 1) {
			if(nestingStack.top()->getNodeType() == IF_) {
				setDummyIfNestingRecovery();
			} else {
				setWhileNestingRecovery();

				head = nestingStack.top();
				nestingStack.pop();
			}
		} else /* i == 1 */ {
			if(nestingStack.top()->getNodeType() == IF_) {
				setEndProcOrEndNodeIfNest(endNode);
			} else {
				setWhileNestingRecovery();

				nestingStack.top()->setSecondChild(endNode);
				endNode->setFirstParent(nestingStack.top());

			}

			nestingStack.pop();
		}
	}

	if(diff == 0) {
		head->setFirstChild(endNode);
		endNode->setFirstParent(head);
	}

	if(currentProcedure != NULL) {
		cfg->addProcedure(currentProcedure);
	}

	head = endNode;
}

void CFGBuilder::setDummyIfNestingRecovery() {
	DummyGNode* dumNode = new DummyGNode();

	dumNode->setIfParentStmt(ifStack.top()->getEndStmt());
	dumNode->setElseParentStmt(head->getEndStmt());

	dumNode->setFirstParent(ifStack.top());
	dumNode->setSecondParent(head);

	setRecoverChild(ifStack.top(), dumNode);
	setRecoverChild(head, dumNode);

	head = dumNode;
	ifStack.pop();
	nestingStack.pop();
}

void CFGBuilder::setWhileNestingRecovery() {
	if(head->getNodeType() == ASSIGN_ || head->getNodeType() == DUMMY_ || head->getNodeType() == CALL_) {
		head->setFirstChild(nestingStack.top());
	} else {
		head->setSecondChild(nestingStack.top());
	}
	nestingStack.top()->setSecondParent(head);
	addNextAndPrev(nestingStack.top(), head);
}

void CFGBuilder::setNodeSameRecoveryIfNest(GNode* node) {
	node->setFirstParent(ifStack.top());
	node->setSecondParent(head);

	setRecoverChild(ifStack.top(), node);
	setRecoverChild(head, node);

	addNextAndPrev(node, ifStack.top());
	addNextAndPrev(node, head);

	head = node;
	ifStack.pop();
	nestingStack.pop();
}

void CFGBuilder::setEndProcOrEndNodeIfNest(EndGNode* node) {
	node->setFirstParent(ifStack.top());
	node->setSecondParent(head);

	setRecoverChild(ifStack.top(), node);
	setRecoverChild(head, node);

	ifStack.pop();
}

void CFGBuilder::addNextAndPrev(GNode* next, GNode* prev) {
	StmtTable* stmtTable = StmtTable::getInstance();

	if(prev->getNodeType() == DUMMY_ && next->getNodeType() != DUMMY_) {
		int nextStmt = next->getStartStmt();
		int prevStmt1 = prev->getStartStmt();
		int prevStmt2 = prev->getEndStmt();

		Statement* nextStatement = stmtTable->getStmtObj(nextStmt);
		unordered_set<int> nextSetAdded = nextStatement->getPrev();
		nextSetAdded.insert(prevStmt1);
		nextSetAdded.insert(prevStmt2);
		nextStatement->setPrev(nextSetAdded);

		Statement* prevStatement1 = stmtTable->getStmtObj(prevStmt1);
		unordered_set<int> prevSetAdded1 = prevStatement1->getNext();
		prevSetAdded1.insert(nextStmt);
		prevStatement1->setNext(prevSetAdded1);

		Statement* prevStatement2 = stmtTable->getStmtObj(prevStmt2);
		unordered_set<int> prevSetAdded2 = prevStatement2->getNext();
		prevSetAdded2.insert(nextStmt);
		prevStatement2->setNext(prevSetAdded1);
	} else if(prev->getNodeType() != DUMMY_ && next->getNodeType() != DUMMY_) {
		int nextStmt = next->getStartStmt();
		int prevStmt = prev->getEndStmt();

		Statement* nextStatement = stmtTable->getStmtObj(nextStmt);
		unordered_set<int> nextSetAdded = nextStatement->getPrev();
		nextSetAdded.insert(prevStmt);
		nextStatement->setPrev(nextSetAdded);

		Statement* prevStatement = stmtTable->getStmtObj(prevStmt);
		unordered_set<int> prevSetAdded = prevStatement->getNext();
		prevSetAdded.insert(nextStmt);
		prevStatement->setNext(prevSetAdded);
	}
}

void CFGBuilder::addNextAndPrev(int next, int prev) {
	StmtTable* stmtTable = StmtTable::getInstance();

	Statement* previousStmt = stmtTable->getStmtObj(prev);
	unordered_set<int> prevNextSet = previousStmt->getNext();
	prevNextSet.insert(next);
	previousStmt->setNext(prevNextSet);

	Statement* nextStmt = stmtTable->getStmtObj(next);
	unordered_set<int> nextPrevSet = nextStmt->getPrev();
	nextPrevSet.insert(prev);
	nextStmt->setPrev(nextPrevSet);
}

void CFGBuilder::setRecoverChild(GNode* parent, GNode* child) {
	if(parent->getNodeType() == WHILE_) {
		parent->setSecondChild(child);
	} else {
		parent->setFirstChild(child);
	}
}

void CFGBuilder::setRecoverParent(GNode* parent, GNode* child) {
	if(child->getNodeType() == WHILE_) {
		child->setSecondParent(parent);
	} else {
		child->setFirstParent(parent);
	}
}

void CFGBuilder::checkPrevStateAndSetProperties(GNode* node) {
	node->setFirstParent(head);
	if(prevState == IF || prevState == WHILE || prevState == CALL) {
		head->setFirstChild(node);
		addNextAndPrev(node, head);
	} else if(prevState == ELSE) {
		head->setSecondChild(node);
		addNextAndPrev(node, head);
	} else /* dummy */{
		head->setFirstChild(node);
	}
	head = node;
}

void CFGBuilder::setStatementReference(GNode* node) {
	StmtTable* stmtTable = StmtTable::getInstance();
	Statement* stmt = stmtTable->getStmtObj(stmtCounter);
	stmt->setGNodeRef(node);
}

void CFGBuilder::setProcReference(ProcGNode* node) {
	ProcTable* procTable = ProcTable::getInstance();
	Procedure* proc = procTable->getProcObj(node->getName());
	proc->setGNodeRef(node);
}

// for testing
string CFGBuilder::getCurrProc() {
	return this->currentProcedure->getName();
}

GNode* CFGBuilder::getHead() {
	return this->head;
}

int CFGBuilder::getStmtCounter() {
	return this->stmtCounter;
}

int CFGBuilder::getNestingLevel() {
	return this->currNestingLevel;
}

CFGBuilder::PrevState CFGBuilder::getPrevState() {
	return this->prevState;
}

stack<GNode*> CFGBuilder::getNestingStack() {
	return this->nestingStack;
}

stack<GNode*> CFGBuilder::getIfStack() {
	return this->ifStack;
}
