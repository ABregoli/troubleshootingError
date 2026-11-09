#include <WinSock2.h>

#pragma comment(lib, "Winmm.lib" )
#pragma comment(lib, "wsock32.lib" )
#pragma comment(lib, "glu32.lib" )
#pragma comment(lib, "uuid.lib" )
#pragma comment(lib, "comctl32.lib" )
#pragma comment(lib, "ws2_32.lib" )
#pragma comment(lib, "Gdiplus.lib" )

#include <sqlite3.h>
#include <string> 
#include <iostream>
#include <vector>
#include <sstream>

#include "UiTroubleshootingEngine.h"
#include "SqlObj.h"

#include <FL/fl_ask.H>
#include <FL/Fl_Hold_Browser.H>

#include <filesystem>



sqlite3 *database;


#include "UiTable.h"

/** @brief metodo di lettura dell'allarme
*/
Alarm findByCode(sqlite3 *database, const std::string &code)
{
    const char* sql = \
    "SELECT id,code,title," \
    "description,severity " \
    "FROM alarms " \
    "WHERE code=?";

    sqlite3_stmt* stmt = nullptr;


    sqlite3_prepare_v2(database, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, code.c_str(), -1, SQLITE_TRANSIENT);
    
    Alarm alarm = {0};

    if(sqlite3_step(stmt) == SQLITE_ROW){
        alarm.id = sqlite3_column_int(stmt,0);
        alarm.code = reinterpret_cast<const char*>(sqlite3_column_text(stmt,1));
        alarm.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt,2));
        alarm.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt,3));
        alarm.severity = sqlite3_column_int(stmt,4);

        sqlite3_finalize(stmt);
        return alarm;
    }
    
    sqlite3_finalize(stmt);
    return alarm;
}

/** @brief metodo di lettura della procedura
*/
Procedure loadFromAlarmId(sqlite3* db, int alarmId)
{
    Procedure procedure = {0};

    const char* sql =
        "SELECT "
        "p.id, "
        "p.name, "
        "s.id, "
        "s.step_order, "
        "s.instruction, "
        "s.escalation_required, "
        "s.escalation_role "
        "FROM procedures p "
        "JOIN alarm_procedure ap "
        "ON p.id = ap.procedure_id "
        "JOIN procedure_steps s "
        "ON p.id = s.procedure_id "
        "WHERE ap.alarm_id=? "
        "ORDER BY s.step_order";

    sqlite3_stmt* stmt = nullptr;

    if(sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK){
        return procedure;
    }

    sqlite3_bind_int(stmt, 1, alarmId);

    bool firstRow = true;

    while(sqlite3_step(stmt) == SQLITE_ROW)
    {
        if(firstRow)
        {
            procedure.id = sqlite3_column_int(stmt,0);
            procedure.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt,1));
            firstRow = false;
        }

        ProcedureStep step;

        step.id = sqlite3_column_int(stmt,2);
        step.order = sqlite3_column_int(stmt,3);
        step.instruction = reinterpret_cast<const char*>(sqlite3_column_text(stmt,4));
        step.escalationRequired = sqlite3_column_int(stmt,5) != 0;
        const unsigned char* roleText = sqlite3_column_text(stmt,6);

        if(roleText){
            step.escalationRole = reinterpret_cast<const char*>(roleText);
        }
        procedure.steps.push_back(step);
    }

    sqlite3_finalize(stmt);

    return procedure;
}

bool insertAlarm(sqlite3* db, const Alarm& alarm)
{
    const char* sql =
        "INSERT OR IGNORE INTO alarms "
        "(code,title,description,severity) "
        "VALUES (?,?,?,?)";

    sqlite3_stmt* stmt = nullptr;

    if(sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK){
        return false;
    }

    sqlite3_bind_text(stmt, 1, alarm.code.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, alarm.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, alarm.description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, alarm.severity);
    bool success = sqlite3_step(stmt) == SQLITE_DONE;

    sqlite3_finalize(stmt);

    return success;
}

bool insertProcedure(sqlite3* db, const std::string& name, int& procedureId)
{
    const char* sql =
        "INSERT OR IGNORE INTO procedures(name)"
        " VALUES(?)";

    sqlite3_stmt* stmt = nullptr;

    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;

    if(success){
        procedureId = static_cast<int>(sqlite3_last_insert_rowid(db));
    }

    sqlite3_finalize(stmt);

    return success;
}

bool insertProcedureStep(sqlite3* db, int procedureId, int order, const std::string& title, const std::string& instruction, bool escalation, const std::string& role)
{
    const char* sql =
        "INSERT OR IGNORE INTO procedure_steps "
        "(procedure_id,"
        "step_order,"
        "title,"
        "instruction,"
        "escalation_required,"
        "escalation_role)"
        "VALUES(?,?,?,?,?,?)";

    sqlite3_stmt* stmt = nullptr;

    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, procedureId);
    sqlite3_bind_int(stmt, 2, order);
    sqlite3_bind_text(stmt, 3, title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, instruction.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, escalation ? 1 : 0);

    sqlite3_bind_text(stmt, 6, role.c_str(), -1, SQLITE_TRANSIENT);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);

    return success;
}

bool linkAlarmProcedure(sqlite3* db, int alarmId, int procedureId)
{
    const char* sql =
        "INSERT OR IGNORE INTO alarm_procedure "
        "(alarm_id,procedure_id)"
        "VALUES(?,?)";

    sqlite3_stmt* stmt = nullptr;

    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, alarmId);
    sqlite3_bind_int(stmt, 2, procedureId);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);

    return success;
}

/** Permette di creare le tabelle 
*/
bool executeSQL(sqlite3* db, const char* sql)
{
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);

    if(rc != SQLITE_OK){
        std::cout << "SQLite Error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

/** @brief Carica la lista completa degli allarmi nel browser per il collegamento procedure
 *
 * Versione semplificata: mostra solo l'ID principale (non il nome codice)
 * Usato principalmente per selezionare un allarme dal browser di sinistra e caricare i dettagli
 */
void loadAlarmsSimplified(sqlite3* db, Fl_Hold_Browser* alarmBrowser)
{
    if (alarmBrowser == nullptr) return;

    alarmBrowser->clear();

    const char* sql =
        "SELECT id as id, code as code FROM alarms ORDER BY id"; // ID semplice per selezione browser

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return; // Errore SQL
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char* code = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

        std::stringstream row;

        // Display: codice allarme (ID principale per selezione browser - non nome/codice complesso)
        if (!code || code[0] == '\0' && strlen(code) == 0) {
            continue;
        }

        row << id << " | " << code; // Formato semplice ID|code

        alarmBrowser->add(row.str().c_str());
    }

    sqlite3_finalize(stmt);
}

/** @brief metodo per l'inizializzazione del database
* 
*/
void InitDatabase()
{
    // NOTA: Tabella 'severity' usata solo per valore (0-4) senza FK real
    // Per compatibilità legacy, manteniamo ma senza foreign key errato su severity(id)
    const char* createAlarms =
        R"(
    CREATE TABLE IF NOT EXISTS alarms
    (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        code TEXT UNIQUE NOT NULL,
        title TEXT NOT NULL,
        description TEXT,
        severity INTEGER CHECK(severity >= 0 AND severity <= 4)
    );
    )";

    // Tabella 'severity' ora usata solo per valori (no FK referenziale)
    const char* createSeverity =
        R"(
    CREATE TABLE IF NOT EXISTS severity  
    (
        id INTEGER PRIMARY KEY CHECK(id >= 0 AND id <= 4),
        description TEXT
    );
    )";

    const char* createProcedures =
    R"(
    CREATE TABLE IF NOT EXISTS procedures
    (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        name TEXT NOT NULL
    );
    )";

    const char* createSteps =
    R"(
    CREATE TABLE IF NOT EXISTS procedure_steps
    (
        id INTEGER PRIMARY KEY AUTOINCREMENT,

        procedure_id INTEGER NOT NULL,

        step_order INTEGER NOT NULL,

        title TEXT,

        instruction TEXT,

        expected_result TEXT,

        escalation_required INTEGER DEFAULT 0,

        escalation_role TEXT,
        
        FOREIGN KEY(escalation_role)
        REFERENCES role(user_role),

        FOREIGN KEY(procedure_id)
        REFERENCES procedures(id)
    );
    )";

    const char* createAlarmProcedure =
    R"(
    CREATE TABLE IF NOT EXISTS alarm_procedure
    (
        alarm_id INTEGER NOT NULL,
        procedure_id INTEGER NOT NULL,

        FOREIGN KEY(alarm_id)
        REFERENCES alarms(id),

        FOREIGN KEY(procedure_id)
        REFERENCES procedures(id)
    );
    )";

    const char* createSessions =
    R"(
    CREATE TABLE IF NOT EXISTS diagnostic_sessions
    (
        id INTEGER PRIMARY KEY AUTOINCREMENT,

        alarm_code TEXT,

        start_time DATETIME,

        end_time DATETIME,

        result TEXT
    );
    )";

    const char* createEscalation = 
    R"(
    CREATE TABLE IF NOT EXISTS role
    (
        user_role TEXT,

        department TEXT

    );
    )";

// Tabella createDiagnostics commentata - ora attiva ma non usata nel codice legacy
    const char* createDiagnostics =
    R"(
    CREATE TABLE IF NOT EXISTS diagnostic_sessions
    (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        alarm_code TEXT,
        start_time DATETIME,
        end_time DATETIME,
        result TEXT
    );
    )";


    executeSQL(database, createAlarms);
    executeSQL(database, createSeverity);
    executeSQL(database, createProcedures);
    executeSQL(database, createSteps);
    executeSQL(database, createAlarmProcedure);
    executeSQL(database, createSessions);
    executeSQL(database, createEscalation);
    executeSQL(database, createDiagnostics);

    //Set lists
    InsertEscalationRole->clear();
            
    const char* sql =
    "SELECT user_role "
    "FROM role "
    "ORDER BY user_role";

    sqlite3_stmt* stmt = nullptr;

    if(sqlite3_prepare_v2(database, sql, -1, &stmt, nullptr) == SQLITE_OK){
            while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char* role =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));

        if(role)
            InsertEscalationRole->add(role);
    }

    sqlite3_finalize(stmt);

    if(InsertEscalationRole->size() > 0)
        InsertEscalationRole->value(0);
    }

}

void loadAlarms(sqlite3* db, Fl_Hold_Browser* alarmBrowser)
{
    alarmBrowser->clear();

    const char* sql =
        "SELECT id, code, title "
        "FROM alarms "
        "ORDER BY code";

    sqlite3_stmt* stmt = nullptr;

    if(sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return;

    while(sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);

        const char* code =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

        const char* title =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));

        std::stringstream row;
        row << id << " | "
            << code << " | "
            << title;

        alarmBrowser->add(row.str().c_str());
    }

    sqlite3_finalize(stmt);
}

void loadProcedureList(sqlite3* db, Fl_Hold_Browser* procedureBrowser)
{
    procedureBrowser->clear();

    const char* sql =
        "SELECT id, name "
        "FROM procedures "
        "ORDER BY id";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);

        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

        std::stringstream row;
        row << id << " | "
            << name;

        procedureBrowser->add(row.str().c_str());
    }

    sqlite3_finalize(stmt);
}

/** @brief carica la lista delle procedure nel browser FL_Hold_Browser
 * 
 * Recupera dalle tabella `procedures` + `alarm_procedure`, ordina per nome
 * e aggiungeno stringhe nella forma "id | nome" nel browser.
 * Include anche il contaggio allampi associati per migliore UX.
 */
void loadProcedures(sqlite3* db, Fl_Hold_Browser* procedureBrowser)
{
    if (procedureBrowser == nullptr) return;
    
    procedureBrowser->clear();

    const char* sql =
        "SELECT COALESCE(p.name, 'NOME NON DEFINITO') as name, p.id, COUNT(*) as alarm_count "
        "FROM procedures p "
        "LEFT JOIN alarm_procedure ap ON p.id = ap.procedure_id "
        "GROUP BY p.id "
        "ORDER BY p.name";

    sqlite3_stmt* stmt = nullptr;

    if(sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return; // Errore SQL - non mostrare lista vuota
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        // Campo 0: name (titolo procedura)
        // Campo 1: id (ID procedura)
        // Campo 2: alarm_count (~contaggio allarmi associati, potrebbe essere NULL se LEFT JOIN non trovato)
        
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        
        // Salto se nome vuoto (anche se LEFT JOIN può dare 'NOME NON DEFINITO')
        if (!name || name[0] == '\0' && strlen(name) == 0) 
        {
            continue; 
        }

        int id = sqlite3_column_int(stmt, 1);
        
        // Controlla se hai un campo di conteggio per display aggiuntivo
        int alarmCount = -1;
        const char* alarmCountText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        if (alarmCountText && strcmp(alarmCountText, "0") != 0) {
            alarmCount = atoi(alarmCountText); // conversione stringhe in int per conteggio
        }

        std::stringstream row;
        row << id << " | " << name;
        
        if(alarmCount >= 0) {
            row << " [" << alarmCount << " allarmi]";} // Aggiunge info utile su quanti allarmi sono collegati
            
        procedureBrowser->add(row.str().c_str());
    }

    sqlite3_finalize(stmt);
}

bool UpdateConnectorList = false;

void UpdateUi()
{
    if(WinLinkAlarmProcedure->visible_r()){
        if(!UpdateConnectorList){
            loadAlarms(database, alarmBrowser);
            loadProcedures(database, procedureBrowser);
            UpdateConnectorList = true;
        }
    }else{
        UpdateConnectorList = false;
    }
}

#include "Ui.c"

std::filesystem::path GetExecutableDirectory()
{
    wchar_t buffer[MAX_PATH];

    DWORD length = GetModuleFileNameW(
        NULL, buffer, MAX_PATH
    );

    if (length == 0 || length >= MAX_PATH)
        return {};

    return std::filesystem::path(std::wstring(buffer, length)).parent_path();
}

int main(int argc, char *argv[])
{
    make_window();

    std::string codeAlarm = "";

    /**Autoinstradamento database nella stessa cartella dell'eseguibile 
    * @todo nelle prossime versioni aggiugere la possibilità di aprire un altro database
    * 
    */
    std::string databasePath = GetExecutableDirectory().string();
    databasePath.append((const char*)"\\database.sqlite");

     
    if(sqlite3_open(databasePath.c_str(), &database) != SQLITE_OK)
    {
        std::cout << "Errore apertura database\n";
        return -1;
    }

    InitDatabase();

    loadAlarms(database, alarmListBrowser);
    loadProcedureList(database, procedureListBrowser);

    operatorWindow->show();

    while(operatorWindow->shown()){

        UpdateUi();

        Fl::check();
    }

    return 0;
}