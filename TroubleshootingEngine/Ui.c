#include <sqlite3.h>
#include <string>

int currentProcedureId = 0;
int currentStep = 1;

// ID sessione diagnostica (semplice autoincremento, non PK real)
int diagnosticSessionId = 0;

void loadStep(int procedureId, int*stepOrder)
{
    const char* sql =       
        "SELECT title "
        "FROM procedure_steps "
        "WHERE procedure_id=? "
        "AND step_order=?";

    sqlite3_stmt* stmt = nullptr;

    sqlite3_prepare_v2(database, sql, -1, &stmt, nullptr);

    sqlite3_bind_int(stmt, 1, procedureId);
    sqlite3_bind_int(stmt, 2, *stepOrder);

    if(sqlite3_step(stmt) == SQLITE_ROW)
    {
        stepNumberOutput->value((std::to_string(*stepOrder)).c_str());

        stepTitleOutput->value(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
    }

    sqlite3_finalize(stmt);
}

int startDiagnosticSession(sqlite3* db, const std::string& alarmCode)
{
    const char* sql =
        "INSERT INTO diagnostic_sessions "
        "(alarm_code,start_time) "
        "VALUES(?,datetime('now'))";

    sqlite3_stmt* stmt = nullptr;

    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, alarmCode.c_str(), -1, SQLITE_TRANSIENT);

    if(sqlite3_step(stmt) != SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        return -1;
    }

    sqlite3_finalize(stmt);

    return static_cast<int>(sqlite3_last_insert_rowid(db));
}

extern void OnSerachBtton(Fl_Button *, void *)
{
    const char* code = alarmCodeInput->value();

    //storico ricerche
    startDiagnosticSession(database, code);

    const char* sql =
        "SELECT a.title, ap.procedure_id "
        "FROM alarms a "
        "INNER JOIN alarm_procedure ap "
        "ON a.id = ap.alarm_id "
        "WHERE a.code = ?";

    sqlite3_stmt* stmt = nullptr;

    if(sqlite3_prepare_v2(database, sql, -1, &stmt, nullptr)
       != SQLITE_OK)
        return;

    sqlite3_bind_text(stmt, 1, code, -1, SQLITE_TRANSIENT);

    if(sqlite3_step(stmt) == SQLITE_ROW)
    {
        alarmTitleOutput->value(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));

        currentProcedureId = sqlite3_column_int(stmt, 1);

        currentStep = 1;

        loadStep(currentProcedureId, &currentStep);
    }

    sqlite3_finalize(stmt);
}
extern void OnAzionePrecedente(Fl_Button *, void *)
{
    if(currentStep > 1){
        currentStep--;

        loadStep(currentProcedureId, &currentStep);
    }
}
extern void OnAzioneSuccessiva(Fl_Button *, void *)
{
    currentStep++;

    loadStep(currentProcedureId, &currentStep);
}
// Genera un ID sessione (simile a rowid autoincremento)
int createDiagnosticSession(sqlite3* db, const std::string& alarmCode)
{
    // SQL per ottenere il prossimo ID disponibile dal tabella diagnostic_sessions
    const char* sql = "SELECT COALESCE(MAX(id), 0) + 1 FROM diagnostic_sessions";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return -1;

    int nextId = 0;

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        nextId = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);

    // Inserisci nuova sessione con start_time e alarm_code trackati
    const char* sqlNewSession = 
        "INSERT INTO diagnostic_sessions (alarm_code, start_time) "
        "VALUES (?, datetime('now'))";

    sqlite3_stmt* stmtNew = nullptr;

    if (sqlite3_prepare_v2(db, sqlNewSession, -1, &stmtNew, nullptr) != SQLITE_OK)
    {
        return -1;
    }

    sqlite3_bind_text(stmtNew, 1, alarmCode.c_str(), -1, SQLITE_TRANSIENT);

    int sessionId = static_cast<int>(sqlite3_last_insert_rowid(db));

    sqlite3_finalize(stmtNew);

    diagnosticSessionId = sessionId;

    return sessionId;
}

// Chiude sessione diagnostica e update con result + end_time
void closeDiagnosticSession(sqlite3* db, int sessionId, const std::string& result)
{
    // Aggiorna sessioni già in corso (con time_end e risultato)
    const char* sql =
        "UPDATE diagnostic_sessions s SET end_time = datetime('now') "
        ", result = ? WHERE s.id = ?";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return; // Error DB
    }

    sqlite3_bind_text(stmt, 1, result.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, sessionId);

    if (sqlite3_step(stmt) == SQLITE_DONE)
    {
        // Reset session active (opzionale se non più necessaria)
        diagnosticSessionId = 0;
    }

    sqlite3_finalize(stmt);
}

extern void OnAzioneRisolto(Fl_Button *, void *)
{
    if(currentProcedureId > 0 && diagnosticSessionId > 0)
    {
        closeDiagnosticSession(database, diagnosticSessionId, "RISOLTO");
    }
    else if (currentProcedureId > 0 && diagnosticSessionId == 0)
    {
        // Crea prima sessione per nuove procedure
        createDiagnosticSession(database, std::to_string(currentProcedureId).c_str());
        closeDiagnosticSession(database, diagnosticSessionId, "RISOLTO");
    }
}

extern void OnAzioneNonRisolto(Fl_Button *, void *)
{
    if (currentProcedureId > 0 && diagnosticSessionId > 0)
    {
        closeDiagnosticSession(database, diagnosticSessionId, "NON RISOLTO");
    }
    else if (currentProcedureId > 0 && diagnosticSessionId == 0)
    {
        createDiagnosticSession(database, std::to_string(currentProcedureId).c_str());
        closeDiagnosticSession(database, diagnosticSessionId, "NON RISOLTO");
    }
}

/// @note aggiunge un nuovo allarme all'interno del dataset
extern void OnAggiungiAllarme(Fl_Button *, void *)
{
	Alarm al;
	al.code = InsertAlarmCode->value();
	al.description = InsertAlarmNote->value();
	al.title = InsertAlarmDescription->value();
	int severity = InsertAlarmSeverity->value();
	if(severity > 4) severity = 4;
	al.severity = severity;

	insertAlarm(database, al);
}


/// @brief cerca se esiste la procedura all'interno del database nella tabella procedures
bool procedureExists(sqlite3* db, const std::string& procedureName, int* procedureId = nullptr)
{
    const char* sql =
        "SELECT id "
        "FROM procedures "
        "WHERE name = ?";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_text(stmt, 1, procedureName.c_str(), -1, SQLITE_TRANSIENT);

    bool exists = false;

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        exists = true;

        if (procedureId)
            *procedureId = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);

    return exists;
}

std::vector<ProcedureStep> loadProcedureSteps(sqlite3* db, int procedureId)
{
    std::vector<ProcedureStep> steps;

    const char* sql =
        "SELECT step_order, title, instruction, "
        "escalation_required, escalation_role "
        "FROM procedure_steps "
        "WHERE procedure_id = ? "
        "ORDER BY step_order";

    sqlite3_stmt* stmt = nullptr;

    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, procedureId);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        ProcedureStep step;

        step.order = sqlite3_column_int(stmt, 0);
        step.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        step.instruction = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        step.escalationRequired = sqlite3_column_int(stmt, 3);
        step.escalationRole = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));

        steps.push_back(step);
    }

    sqlite3_finalize(stmt);

    return steps;
}

/**
* @todo inserire anche la ricerca simile alla stringa non esatta
*
*/
void OnFindProcedure(Fl_Button *, void *)
{
	std::string procedureNameStr = procedureName->value();
	int procedureId = 0;
	if(!procedureExists(database, procedureNameStr, &procedureId)){
		if(fl_ask("Procedura %s non presente, aggiugere ?", procedureNameStr.c_str())){
			//Aggiungi nome procedura
			insertProcedure(database, procedureNameStr.c_str(), procedureId);
        } else{
            procedureID->value(0);
            return;
        }
    }

	procedureID->value(procedureId);

    std::vector<ProcedureStep> procedureStep = loadProcedureSteps(database, procedureId);
    tableStepProcedure->setSteps(procedureStep);
    
}

void OnProcedureUpdate(Fl_Button *, void *)
{
    int procedureId = procedureID->value();
    
    if(procedureId == 0){
        fl_alert("attenzione !!!, selezionare una procedura valida e riprovare");
        return;
    }
    
    loadProcedureList(database, procedureListBrowser);

    std::vector<ProcedureStep> procedureStep = loadProcedureSteps(database, procedureId);
    tableStepProcedure->setSteps(procedureStep);
}

void OnProcedureListBrowser(Fl_Hold_Browser *obj, void*)
{
  /// @todo doppio click per completare procedureName
}

void OnAddStepProcedure(Fl_Button *, void *)
{
    int procedure = procedureID->value();

    if(procedure == 0){
        fl_alert("attenzione !!!, selezionare una procedura valida e riprovare");
        return;
    }
    std::string title = InsertProcedureTitle->value();
    if(title.empty()){
        fl_alert("Inserire un titolo e riprovare");
        return;
    }
    std::string instruction = InsertProcedureInstruction->value();
    if(instruction.empty()){
        fl_alert("Inserire un istruzione e riprovare");
        return;
    }

    /// @todo Controllare l'univocità dei passi per non ripetere lo stesso passo piu volte.
    std::string escalationRole = InsertEscalationRole->text();

    insertProcedureStep(database, procedure, InsertProcedurePasso->value(), title, instruction, InsertProcedureEscalation->value(), escalationRole);

    std::vector<ProcedureStep> procedureStep = loadProcedureSteps(database, procedure);
    tableStepProcedure->setSteps(procedureStep);
}

void OnLinkProcedureUpdate(Fl_Button *, void *)
{
    loadAlarms(database, alarmBrowser);
    loadProcedures(database, procedureBrowser);
}

extern void OnAlarmListSelect(Fl_Widget *, void *)
{
    int selected = alarmListBrowser->value();

    if (selected <= 0)
        return;

    const char* row = alarmListBrowser->text(selected);

    // Formato della riga: "id | code | title"
    std::stringstream ss(row);

    int alarmId;
    char sep;
    std::string code;

    ss >> alarmId >> sep >> code;

    if (!code.empty())
    {
        alarmCodeInput->value(code.c_str());
    }
}

int getSelectedAlarmId(Fl_Hold_Browser* alarmBrowser)
{
    int selected = alarmBrowser->value();

    if(selected <= 0)
        return -1;

    const char* row = alarmBrowser->text(selected);

    std::stringstream ss(row);

    int alarmId;
    char sep;

    ss >> alarmId;

    return alarmId;
}

int getSelectedProcedureId(Fl_Hold_Browser* procedureBrowser)
{
    int selected = procedureBrowser->value();

    if(selected <= 0)
        return -1;

    const char* row = procedureBrowser->text(selected);

    std::stringstream ss(row);

    int procedureId;
    ss >> procedureId;

    return procedureId;
}

void OnLinkProcedureCollega(Fl_Button *, void *)
{
    int alarmId = getSelectedAlarmId(alarmBrowser);
    int procedureId = getSelectedProcedureId(procedureBrowser);

    linkAlarmProcedure(database, alarmId, procedureId);
}



/********************************************************
******************MENU***********************************
*********************************************************/
extern void OnAssistenteDiagnostica(Fl_Menu_ *, void *)
{
	for(int ii = 0; ii < operatorWindow->children(); ii++){
		Fl_Widget *wid = operatorWindow->child(ii);
		if(wid->as_group()) wid->hide();
	}
	WinAssistenzaMacchina->show();
}
extern void OnInserimentoDati(Fl_Menu_ *, void *)
{
	for(int ii = 0; ii < operatorWindow->children(); ii++){
		Fl_Widget *wid = operatorWindow->child(ii);
		if(wid->as_group()) wid->hide();
	}

	WinFeedDatabase->show();
}

extern void OnLinkAlarmProcedure(Fl_Menu_ *, void *)
{
    for(int ii = 0; ii < operatorWindow->children(); ii++){
		Fl_Widget *wid = operatorWindow->child(ii);
		if(wid->as_group()) wid->hide();
	}
    WinLinkAlarmProcedure->show();
}