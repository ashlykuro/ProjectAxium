#include "DatabaseEnv.h"
#include "DatabaseWorker.h"
#include "SQLOperation.h"
#include "MySQLConnection.h"
#include "MySQLThreading.h"

DatabaseWorker::DatabaseWorker(ACE_Activation_Queue* new_queue, MySQLConnection* con) :
m_queue(new_queue),
m_conn(con)
{
    /// Assign thread to task
    activate();
}

int DatabaseWorker::svc()
{
    if (!m_queue)
        return -1;

    SQLOperation *request = NULL;
    while (1)
    {
        request = (SQLOperation*)(m_queue->dequeue());
        if (!request)
            break;

        request->SetConnection(m_conn);
        request->call();

        delete request;
    }

    return 0;
}
