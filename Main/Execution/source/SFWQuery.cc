
#ifndef SFW_QUERY_CC
#define SFW_QUERY_CC

#include "ParserTypes.h"
	
// builds and optimizes a logical query plan for a SFW query, returning the logical query plan
pair <LogicalOpPtr, double> SFWQuery :: optimizeQueryPlan (map <string, MyDB_TablePtr> &allTables) {

	// here we call the recursive, exhaustive enum. algorithm
	// return optimizeQueryPlan (...);
}

// builds and optimizes a logical query plan for a SFW query, returning the logical query plan
pair <LogicalOpPtr, double> SFWQuery :: optimizeQueryPlan (map <string, MyDB_TablePtr> &allTables, 
	MyDB_SchemaPtr totSchema, vector <ExprTreePtr> &allDisjunctions) {

	LogicalOpPtr res = nullptr;
	double cost = 9e99;
	double best = 9e99;

	// case where no joins
	if (allTables.size () == 1) {

		// some code here...
		return make_pair (res, best);
	}

	// we have at least one join
	vector<pair<string, MyDB_TablePtr>> tableList;
	tableList.reserve(allTables.size());
	map <string, string> tableAliasMap;

	for (pair <string, string> tableAlias : tablesToProcess) {
		auto entry = allTables.find(tableAlias.first);
		if (entry != allTables.end()) {
			tableList.push_back(*entry);
			tableAliasMap[tableAlias.first] = tableAlias.second;
		}
	}

	int n = tableList.size();
	for (int mask = 1; mask < (1 << (n - 1)); ++mask) {
		if (mask == 0 || mask == (1 << n) - 1) continue;

		map<string, MyDB_TablePtr> leftTables;
    	map<string, MyDB_TablePtr> rightTables;

		for (int i = 0; i < n; ++i) {
			if (mask & (1 << i)) {
				// 1 goes to RIGHT group
				rightTables[tableList[i].first] = tableList[i].second;
			} else {
				// 0 goes to LEFT group
				leftTables[tableList[i].first] = tableList[i].second;
			}
		}

		// find the various parts of the CNF
		vector <ExprTreePtr> leftCNF; 
		vector <ExprTreePtr> rightCNF; 
		vector <ExprTreePtr> topCNF; 

		// loop through all of the disjunctions and break them apart
		for (auto a: allDisjunctions) {
			bool inLeft = false;
			for (pair<string, MyDB_TablePtr> leftTable : leftTables) {
				string alias = tableAliasMap[leftTable.first];
				if (a->referencesTable(alias)) {
					inLeft = true;
					break;
				}
			}

			bool inRight= false;
			for (pair<string, MyDB_TablePtr> rightTable : rightTables) {
				string alias = tableAliasMap[rightTable.first];
				if (a->referencesTable(alias)) {
					inRight= true;
					break;
				}
			}
			
			if (inLeft && inRight) {
				cout << "top " << a->toString () << "\n";
				topCNF.push_back (a);
			} else if (inLeft) {
				cout << "left: " << a->toString () << "\n";
				leftCNF.push_back (a);
			} else {
				cout << "right: " << a->toString () << "\n";
				rightCNF.push_back (a);
			}
		}

		MyDB_SchemaPtr leftSchema = make_shared <MyDB_Schema> ();
		MyDB_SchemaPtr rightSchema = make_shared <MyDB_Schema> ();
		// Not sure if we need combSchema
		// MyDB_SchemaPtr combSchema = make_shared <MyDB_Schema> ();

		for (pair <string, MyDB_TablePtr> leftTable : leftTables) {
			string alias = tableAliasMap[leftTable.first];
			for (auto b: leftTable.second->getSchema ()->getAtts ()) {
				bool needIt = false;
				for (auto a: valuesToSelect) {
					if (a->referencesAtt (alias, b.first)) {
						needIt = true;
						break;
					}
				}

				if (!needIt) {
					for (auto a: topCNF) {
						if (a->referencesAtt (alias, b.first)) {
							needIt = true;
							break;
						}
					}
				}

				if (needIt) {
					leftSchema->getAtts ().push_back (make_pair (alias + "_" + b.first, b.second));
					// combSchema->getAtts ().push_back (make_pair (alias+ "_" + b.first, b.second));
				}
			}
		}

		cout << "left schema: " << leftSchema << "\n";

		for (pair <string, MyDB_TablePtr> rightTable : rightTables) {
			string alias = tableAliasMap[rightTable.first];
			for (auto b: rightTable.second->getSchema ()->getAtts ()) {
				bool needIt = false;
				for (auto a: valuesToSelect) {
					if (a->referencesAtt (alias, b.first)) {
						needIt = true;
						break;
					}
				}

				if (!needIt) {
					for (auto a: topCNF) {
						if (a->referencesAtt (alias, b.first)) {
							needIt = true;
							break;
						}
					}
				}

				if (needIt) {
					rightSchema->getAtts ().push_back (make_pair (alias + "_" + b.first, b.second));
					// combSchema->getAtts ().push_back (make_pair (alias+ "_" + b.first, b.second));
				}
			}
		}

		cout << "right schema: " << leftSchema << "\n";

		pair<LogicalOpPtr, double> leftPlan = optimizeQueryPlan (leftTables, leftSchema, leftCNF);
		pair<LogicalOpPtr, double> rightPlan = optimizeQueryPlan (rightTables, rightSchema, rightCNF);

		// Not sure what to name the output table
		MyDB_TablePtr outTable = make_shared <MyDB_Table> ("tempTable", "tempTableLoc", totSchema);
		// Not sure if this is the stats to get
		MyDB_StatsPtr finalStats = leftPlan.first->getStats ()->costJoin (topCNF, rightPlan.first->getStats ());
		LogicalOpPtr myExp = make_shared <LogicalJoin> (leftPlan.first, rightPlan.first, outTable, topCNF, finalStats);

		cost = leftPlan.second + rightPlan.second + finalStats->getTupleCount ();

		if (cost < best) {
			best = cost;
			res = myExp;
		}
	}

	return make_pair (res, best);
}

void SFWQuery :: print () {
	cout << "Selecting the following:\n";
	for (auto a : valuesToSelect) {
		cout << "\t" << a->toString () << "\n";
	}
	cout << "From the following:\n";
	for (auto a : tablesToProcess) {
		cout << "\t" << a.first << " AS " << a.second << "\n";
	}
	cout << "Where the following are true:\n";
	for (auto a : allDisjunctions) {
		cout << "\t" << a->toString () << "\n";
	}
	cout << "Group using:\n";
	for (auto a : groupingClauses) {
		cout << "\t" << a->toString () << "\n";
	}
}


SFWQuery :: SFWQuery (struct ValueList *selectClause, struct FromList *fromClause,
        struct CNF *cnf, struct ValueList *grouping) {
        valuesToSelect = selectClause->valuesToCompute;
        tablesToProcess = fromClause->aliases;
        allDisjunctions = cnf->disjunctions;
        groupingClauses = grouping->valuesToCompute;
}

SFWQuery :: SFWQuery (struct ValueList *selectClause, struct FromList *fromClause,
        struct CNF *cnf) {
        valuesToSelect = selectClause->valuesToCompute;
        tablesToProcess = fromClause->aliases;
	allDisjunctions = cnf->disjunctions;
}

SFWQuery :: SFWQuery (struct ValueList *selectClause, struct FromList *fromClause) {
        valuesToSelect = selectClause->valuesToCompute;
        tablesToProcess = fromClause->aliases;
        allDisjunctions.push_back (make_shared <BoolLiteral> (true));
}

#endif
