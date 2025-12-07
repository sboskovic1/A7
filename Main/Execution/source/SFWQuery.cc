
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
	double best = std::numeric_limits<double>::infinity();

	// case where no joins
	if (allTables.size () == 1) {

		// some code here...
		return make_pair (res, best);
	}

	// we have at least one join
	vector<pair<string, MyDB_TablePtr>> tableList;
	vector<pair <string, string>> tableAliases;
	tableList.reserve(allTables.size());
	tableAliases.reserve(allTables.size());

	for (pair <string, string> tableAlias : tablesToProcess) {
		auto entry = allTables.find(tableAlias.first);
		if (entry != allTables.end()) {
			tableList.push_back(*entry);
			tableAliases.push_back(tableAlias);
		}
	}

	int n = tableList.size();
	for (int mask = 1; mask < (1 << (n - 1)); ++mask) {
		if (mask == 0 || mask == (1 << n) - 1) continue;

		map<string, MyDB_TablePtr> leftTables;
    	map<string, MyDB_TablePtr> rightTables;
		vector<pair <string, string>> leftAliases;
		vector<pair <string, string>> rightAliases;

		for (int i = 0; i < n; ++i) {
			if (mask & (1 << i)) {
				// 1 goes to RIGHT group
				rightTables[tableList[i].first] = tableList[i].second;
				rightAliases.push_back(tableAliases[i]);
			} else {
				// 0 goes to LEFT group
				leftTables[tableList[i].first] = tableList[i].second;
				leftAliases.push_back(tableAliases[i]);
			}
		}

		// find the various parts of the CNF
		vector <ExprTreePtr> leftCNF; 
		vector <ExprTreePtr> rightCNF; 
		vector <ExprTreePtr> topCNF; 

		// loop through all of the disjunctions and break them apart
		for (auto a: allDisjunctions) {
			bool inLeft = false;
			for (pair<string, string> leftAlias : leftAliases) {
				if (a->referencesTable(leftAlias.second)) {
					inLeft = true;
					break;
				}
			}

			bool inRight= false;
			for (pair<string, string> rightAlias : rightAliases) {
				if (a->referencesTable(rightAlias.second)) {
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
