
#ifndef SFW_QUERY_CC
#define SFW_QUERY_CC

#include "ParserTypes.h"
	
// builds and optimizes a logical query plan for a SFW query, returning the logical query plan
pair <LogicalOpPtr, double> SFWQuery :: optimizeQueryPlan (map <string, MyDB_TablePtr> &allTables) {

	// here we call the recursive, exhaustive enum. algorithm

	MyDB_SchemaPtr totSchema = make_shared <MyDB_Schema> ();
	map <string, MyDB_TablePtr> allTablesNeeded;
	for (auto &a : tablesToProcess) {
		allTablesNeeded[a.second] = allTables[a.first];
	}

	for (const auto& pair : allTablesNeeded) {
		string alias = pair.first;
		MyDB_TablePtr table = pair.second;

		for (auto b : table->getSchema ()->getAtts ()) {
			bool needIt = false;
			for (auto a: valuesToSelect) {
				if (a-> referencesAtt (alias, b.first)) {
					needIt = true;
					break;
				}
			}

			if (needIt) {
				totSchema->getAtts ().push_back (make_pair (alias + "_" + b.first, b.second));
			}
		}
	}

	cout << "total schema: " << totSchema << "\n";
	return optimizeQueryPlan (allTablesNeeded, totSchema, allDisjunctions);
}

// builds and optimizes a logical query plan for a SFW query, returning the logical query plan
pair <LogicalOpPtr, double> SFWQuery :: optimizeQueryPlan (map <string, MyDB_TablePtr> &allTables, 
	MyDB_SchemaPtr totSchema, vector <ExprTreePtr> &allDisjunctions) {

	cout << "Inside recursive function, total number of tables: " << allTables.size() << endl;

	LogicalOpPtr res = nullptr;
	double cost = 9e99;
	double best = 9e99;

	// case where no joins
	if (allTables.size () == 1) {
		cout << "We've hit the base case" << endl;
		cout << "total schema: " << totSchema << endl;

		cout << "all disjunctions: " << endl;
		for (auto a: allDisjunctions) {
			cout << a->toString() << endl;
		}

		auto it = allTables.begin();
		string tableName = it->first;
		cout << "tableName: " << tableName << endl;

		MyDB_TablePtr table = it->second;
		MyDB_TablePtr aliasTable = table->alias(tableName);


		MyDB_TablePtr outTable = make_shared <MyDB_Table> ("tempTable" + to_string(name), "tempTableLoc" + to_string(name), totSchema);
		name++;
		MyDB_StatsPtr stats = make_shared <MyDB_Stats> (aliasTable);
		cout << "All stats all atts: " << endl;
		stats->print();
		cout << "Performing cost selection" << endl;
		MyDB_StatsPtr scanStats = stats->costSelection(allDisjunctions);
		
		LogicalOpPtr myExp = make_shared <LogicalTableScan> (aliasTable, outTable, scanStats, allDisjunctions);
		res = myExp;
		double tupleCount = scanStats->getTupleCount();
		cout << "tupleCount: " << tupleCount << endl;
		best = tupleCount;
		
		cout << "Best returned from base case: " << best << endl;
		return make_pair (res, best);
	}

	// we have at least one join
	vector<pair<string, MyDB_TablePtr>> tableList;
	tableList.reserve(allTables.size());

	for (const auto& pair : allTables) {
		string tableName = pair.first;
		MyDB_TablePtr table = pair.second;

		tableList.push_back(make_pair(tableName, table));
	}

	int n = tableList.size();
	for (int mask = 1; mask < (1 << (n - 1)); ++mask) {
		if (mask == 0 || mask == (1 << n) - 1) continue;

		map<string, MyDB_TablePtr> leftTables;
    	map<string, MyDB_TablePtr> rightTables;

		for (int i = 0; i < n; ++i) {
			if (mask & (1 << i)) {
				// 1 goes to RIGHT group
				cout << tableList[i].first << "going in right table" << endl;
				rightTables[tableList[i].first] = tableList[i].second;
			} else {
				// 0 goes to LEFT group
				leftTables[tableList[i].first] = tableList[i].second;
				cout << tableList[i].first << "going in left table" << endl;
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
				if (a->referencesTable(leftTable.first)) {
					inLeft = true;
					break;
				}
			}

			bool inRight= false;
			for (pair<string, MyDB_TablePtr> rightTable : rightTables) {
				if (a->referencesTable(rightTable.first)) {
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

		for (pair <string, MyDB_TablePtr> leftTable : leftTables) {
			string alias = leftTable.first;
			for (auto b: leftTable.second->getSchema ()->getAtts ()) {
				bool needIt = false;
				for (auto a: totSchema->getAtts()) {
					if (alias + "_" + b.first == a.first) {
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
				}
			}
		}

		cout << "left schema: " << leftSchema << "\n";

		for (pair <string, MyDB_TablePtr> rightTable : rightTables) {
			string alias = rightTable.first;
			for (auto b: rightTable.second->getSchema ()->getAtts ()) {
				bool needIt = false;
				for (auto a: totSchema->getAtts()) {
					if (alias + "_" + b.first == a.first) {
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
				}
			}
		}

		cout << "right schema: " << leftSchema << "\n";

		pair<LogicalOpPtr, double> leftPlan = optimizeQueryPlan (leftTables, leftSchema, leftCNF);
		pair<LogicalOpPtr, double> rightPlan = optimizeQueryPlan (rightTables, rightSchema, rightCNF);

		MyDB_TablePtr outTable = make_shared <MyDB_Table> ("tempTable" + to_string(name), "tempTableLoc" + to_string(name), totSchema);
		name++;

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
		name = 1;
}

SFWQuery :: SFWQuery (struct ValueList *selectClause, struct FromList *fromClause,
        struct CNF *cnf) {
        valuesToSelect = selectClause->valuesToCompute;
        tablesToProcess = fromClause->aliases;
		allDisjunctions = cnf->disjunctions;
		name = 1;
}

SFWQuery :: SFWQuery (struct ValueList *selectClause, struct FromList *fromClause) {
        valuesToSelect = selectClause->valuesToCompute;
        tablesToProcess = fromClause->aliases;
        allDisjunctions.push_back (make_shared <BoolLiteral> (true));
		name = 1;
}

#endif
