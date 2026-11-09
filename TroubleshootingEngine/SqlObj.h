#pragma once

#include <string>
#include <vector>

#include <sqlite3.h>


struct Alarm{
    int id;
    std::string code;
    std::string title;
    std::string description;
    int severity;
};

struct ProcedureStep
{
    int id;
    int order;
    std::string title;
    std::string instruction;
    bool escalationRequired;
    std::string escalationRole;
};

struct Procedure
{
    int id;
    std::string name;
    std::vector<ProcedureStep> steps;
};


