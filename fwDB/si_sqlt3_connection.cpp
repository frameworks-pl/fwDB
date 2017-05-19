#include "stdafx.h"
#include "SICryptException.h"

#ifdef _DEBUG
//#define new DEBUG_NEW
#endif

namespace sidb
{

SI_SQLT3_Connection::SI_SQLT3_Connection()
{

  m_pCnnHandle = NULL;


}


void SI_SQLT3_Connection::query(const CString& pQuery)
{
	if (NULL != m_pCnnHandle)
	{
		std::string utf8Query = Unicode2UTF8(pQuery);
        beginTransaction();
		if (SQLITE_OK != sqlite3_exec(m_pCnnHandle, utf8Query.c_str(), NULL, NULL, NULL))
		{
			throw SIDBException(m_pCnnHandle);
		}
        endTransaction();
	}
	else
	{
		throw SIDBException(_T("Database connection is invalid."));
	}

}


void SI_SQLT3_Connection::createDatabase(const CString& pFilePath, const CString& pScript)
{
  std::string utfPath = Unicode2UTF8(pFilePath);
  if (SQLITE_OK != sqlite3_open_v2(utfPath.c_str(), &m_pCnnHandle, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL))
  {
    //throw SIDBException(_T("Failed to open/create database."));
    throw SIDBException(m_pCnnHandle);
  }

  if (isOpen())
    query(pScript);

}


void SI_SQLT3_Connection::openConnection(const CString& pFilePath)
{

  if (sicommon::SIFileUtils::fileExists(pFilePath))
  {
    std::string stdPath = Unicode2UTF8(pFilePath);
    //sqlite3_open_v2 is used to do not allow creation of database if 
    //it does not exists!
    if (SQLITE_OK != sqlite3_open_v2(stdPath.c_str(), &m_pCnnHandle, SQLITE_OPEN_READWRITE, NULL))
    {
      //TODO: detect error in more detail
      throw SIDBException(_T("Failed to open database."));
    }
    else
    {
      //store path to the database
      m_sPath = pFilePath;
    }
  }
  else
  {
    CString sMsg;
    sMsg.Format(_T("File %s not found"), pFilePath);
    throw SIDBException(sMsg);
  }

}


void SI_SQLT3_Connection::closeConnection()
{
  if (m_pCnnHandle)
  {
    int iResult = sqlite3_close(m_pCnnHandle);
    if (SQLITE_OK != iResult)
    {
        TRACE(_T("ERROR: Closing database failed with result %d\n"), iResult);
    }
    m_pCnnHandle = NULL;
    m_sPath = _T("");
  }
  else
  {
    throw SIDBException(_T("Cannot close the database - handle is invalid"));
  }
}


void SI_SQLT3_Connection::query(SISQLRowDataSet& pRowDataset)
{
  if (m_pCnnHandle)
  {
    CString sQuery = pRowDataset.getReadQuery();
    if (sQuery.IsEmpty())
      throw SIDBException(_T("Query string is empty."));

    std::string utf8Query = Unicode2UTF8(sQuery);
    char szErrorMsg[1024];
    char* p = szErrorMsg;

    sqlite3_exec(m_pCnnHandle, utf8Query.c_str(), QueryCallbackRowDataSet, (void*)&pRowDataset, &p);
  }
  else
  {
    throw SIDBException(_T("Database is not open."));
  }  
}


void SI_SQLT3_Connection::query(SISQLObject* pObject, bool bWrite, SQL_ID sqlID)
{

    if (NULL != m_pCnnHandle && NULL != pObject)
    {

        int iParamsCount;
        sqlite3_stmt* pStmt = NULL;
        int rc;
        std::string utf8Query; 

        if (false == bWrite)
        {
            //the object must have a valid sql ID
            SQL_ID id = sqlID;
            if (INVALID_SQL_ID == id)
            {
                //maybe object has valid id 
                id = pObject->getSQLID();

                //no luck (don't know what to load
                if (INVALID_SQL_ID == id)
                    throw SIDBException(_T("The object has invalid id."));
            }

            CString sReadQuery;
            iParamsCount = pObject->getUpdateQuery(sReadQuery, bWrite, false);

            //adding WHERE
            CString sWhere;
            sWhere.Format(_T(" WHERE sql_id = %d"), (int)id);
            sReadQuery += sWhere;

            if (sReadQuery.IsEmpty())
                throw SIDBException(_T("Query string is empty."));
            utf8Query = Unicode2UTF8(sReadQuery);

#ifdef ALLOW_PERFORMANCE_ISSUES
            SILog::Log(LEVEL_SIDB_PREPARE_FINALIZE, "{SI_SQLT3_Connection::query} prepare (%s, %d)", __FILE__, __LINE__);
#endif //ALLOW_PERFORMANCE_ISSUES
            rc = sqlite3_prepare(m_pCnnHandle, utf8Query.c_str(), (int)utf8Query.size(), &pStmt, NULL);
            if (SQLITE_OK != rc)
                throw SIDBException(m_pCnnHandle);

            rc = sqlite3_step(pStmt);
            if (SQLITE_ROW == rc)
            {
                readRecord(pStmt, pObject);

                //no longer initializing objects in this method
                //now caller is responsible for intializing them

                //if (true == pObject->initialize())
                //{
                //    pObject->setState(SISQLObject::STATE_OK);
                //}
                //else
                //{
                //    throw SIDBException(_T("At least one object failed to initialize."));
                //}
                rc = sqlite3_step(pStmt);
                if (SQLITE_DONE != rc)
                    throw SIDBException(_T("Too many items read for single oject"));
            }
            else
            {
                throw SIDBException(m_pCnnHandle);
            } 
#ifdef ALLOW_PERFORMANCE_ISSUES
            SILog::Log(LEVEL_SIDB_PREPARE_FINALIZE, "{SI_SQLT3_Connection::query} finalize (%s, %d)", __FILE__, __LINE__);
#endif //ALLOW_PERFORMANCE_ISSUES
            rc = sqlite3_finalize(pStmt);
        }
        else
        {
            CString sWriteQuery;
            iParamsCount = pObject->getUpdateQuery(sWriteQuery, bWrite, false);

            //adding WHERE
            CString sWhere;
            sWhere.Format(_T(" WHERE sql_id = %d"), (int)pObject->getSQLID());
            sWriteQuery += sWhere;

            utf8Query = Unicode2UTF8(sWriteQuery); 
#ifdef ALLOW_PERFORMANCE_ISSUES
            SILog::Log(LEVEL_SIDB_PREPARE_FINALIZE, "{SI_SQLT3_Connection::query} prepare (%s, %d)", __FILE__, __LINE__);
#endif //ALLOW_PERFORMANCE_ISSUES
            rc = sqlite3_prepare(m_pCnnHandle, utf8Query.c_str(), (int)utf8Query.size(), &pStmt, NULL);

            if (SQLITE_OK != rc)
                throw SIDBException(m_pCnnHandle);

            try
            {
                writeRecord(pStmt, pObject, iParamsCount);
                //running the query
                rc = sqlite3_step(pStmt);
                if (rc == SQLITE_ROW)
                    throw SIDBException(_T("Non select query returned row(s)."));

#ifdef ALLOW_PERFORMANCE_ISSUES
            SILog::Log(LEVEL_SIDB_PREPARE_FINALIZE, "{SI_SQLT3_Connection::query} finalize (%s, %d)", __FILE__, __LINE__);
#endif //ALLOW_PERFORMANCE_ISSUES
                rc = sqlite3_finalize(pStmt);
            }
            catch (SIDBException& ex)
            {
#ifdef ALLOW_PERFORMANCE_ISSUES
            SILog::Log(LEVEL_SIDB_PREPARE_FINALIZE, "{SI_SQLT3_Connection::query} finalize (%s, %d)", __FILE__, __LINE__);
#endif //ALLOW_PERFORMANCE_ISSUES
                sqlite3_finalize(pStmt);
                throw ex;
            }
        }
    }

}

void SI_SQLT3_Connection::readRecord(sqlite3_stmt* pStmt, SISQLObject* pSQLObject)
{

    int iColumnCnt = sqlite3_column_count(pStmt);            
    if ((NULL != pSQLObject) && iColumnCnt > 0)
    {
        for (int iCol = 0; iCol < iColumnCnt; iCol++)
        {
        int iColType = sqlite3_column_type(pStmt, iCol);
        const char* pColName = sqlite3_column_name(pStmt, iCol);

        CString sColName = UTF82Unicode(pColName);

        switch (iColType)
        {
            case SQLITE_INTEGER:
            {
                int iVal = sqlite3_column_int(pStmt, iCol);
                pSQLObject->updateFromInt(sColName, iVal);
            }
            break;
            case SQLITE_BLOB:
            {
                const void* pBlob = sqlite3_column_blob(pStmt, iCol);
                int iBytes = sqlite3_column_bytes(pStmt, iCol);
                fw::core::ByteBuffer bb(iBytes);
                memcpy((void*)bb.getBuffer(), pBlob, iBytes);                                            
                pSQLObject->updateFromBLOB(sColName, bb);
            }
            break;
        }
        }

        //forcing STATE_OK on the loaded object
        pSQLObject->setState(SISQLObject::STATE_OK_LOADED);

    }
    else
    {
        throw SIDBException(_T("Failed to create object during read operation."));
    }
}

void SI_SQLT3_Connection::writeRecord(sqlite3_stmt* pStmt, SISQLObject* pSQLObject, int iParamsCount)
{

    //binding blobs
    for (int i = 1; i <= iParamsCount; i++)
    {
        const SISQLParam& pParamToBind = pSQLObject->getParamToBind(i);
        if (pParamToBind.isValid())
        sqlite3_bind_blob(pStmt, i, (void*)pParamToBind.getBLOBBuffer().getBuffer(), 
        pParamToBind.getBLOBBuffer().getLength(), SQLITE_STATIC);
        else
        throw SIDBException(_T("Param to bind is invalid."));
    }                    


}


int SI_SQLT3_Connection::query(SISQLObjectDataSet& pObjectDataset, bool bWrite, const CString& pWherePhrase, bool bClearCollection)
{

 int iObjectsCount = 0;
 bool bFinalized = true;
 sqlite3_stmt* pStmt = NULL;


 try
 {
    if (m_pCnnHandle)
    {

        int iParamsCount;        
        int rc;
        std::string utf8Query;    

        if (false == bWrite)
        {

            //clear the dataset
            if (true == bClearCollection)
                pObjectDataset.clear();
            unsigned char *pzBlob = NULL;
            int pnBlob = 0;

            CString sReadQuery;
            iParamsCount = pObjectDataset.getReadQuery(sReadQuery);

            if (sReadQuery.IsEmpty())
                throw SIDBException(_T("Query string is empty."));

            if (false == pWherePhrase.IsEmpty())
            {
                sReadQuery += " WHERE ";
                sReadQuery += pWherePhrase;
            }


            utf8Query = Unicode2UTF8(sReadQuery);
#ifdef ALLOW_PERFORMANCE_ISSUES
            SILog::Log(LEVEL_SIDB_PREPARE_FINALIZE, "{SI_SQLT3_Connection::query} prepare (%s, %d)", __FILE__, __LINE__);
#endif //ALLOW_PERFORMANCE_ISSUES
            rc = sqlite3_prepare(m_pCnnHandle, utf8Query.c_str(), (int)utf8Query.size(), &pStmt, NULL);
            bFinalized = false;
#ifdef ALLOW_PERFORMANCE_ISSUES
                SILog::Log(LEVEL_SIDB_SQLITE, "{SI_SQLT3_Connection::query} sqlite_prepare (result=%d)\n", rc);
#endif //ALLOW_PERFORMANCE_ISSUES
            if (SQLITE_OK != rc)
            {
            throw SIDBException(m_pCnnHandle);
            }

            do
            {

                //running the query
                rc = sqlite3_step(pStmt);

                switch (rc)
                {
                case SQLITE_ROW:
                {
                    //create dummy object to perform the load operation 
                    SISQLObject* pSQLObject = pObjectDataset.createObject();

                    if (NULL != pSQLObject)
                    {
                        readRecord(pStmt, pSQLObject);

                        //adding loaded item to the collection
                        try
                        {
                            pObjectDataset.set(pSQLObject);
                        }
                        catch (sicrypt::SICryptException& ex)
                        {
							delete pSQLObject;
							pSQLObject = NULL;
                            throw SIDBException(_T("Decryption failed during initialization."));
                        }
                        catch (fw::core::Exception& ex)
                        {
							delete pSQLObject;
							pSQLObject = NULL;

                            throw SIDBException(m_pCnnHandle);
                        }

                        iObjectsCount++;

                        //notify listeners about objects count change
                        std::list<fw::core::GenericListener*>::iterator listenerIt;
                        for (listenerIt = m_Listeners.begin(); listenerIt != m_Listeners.end(); listenerIt++)
                        ((SI_SQLT3_Connection_Listener*)(*listenerIt))->onUpdatedItemsCountChange(iObjectsCount);

                        delete pSQLObject;
                        pSQLObject = NULL;
                    }
                    else
                    {
                    throw SIDBException(_T("Failed to create object during read operation."));
                    }
                }
                break;
                case SQLITE_MISUSE:
                case SQLITE_ERROR:
                    throw SIDBException(m_pCnnHandle);
                break;
                case SQLITE_DONE:
                break;
                default:
                    throw SIDBException(_T("Unknown value returned while processing query."));
                }
             
            }
            while (rc == SQLITE_ROW);

            bFinalized = true;
#ifdef ALLOW_PERFORMANCE_ISSUES
            SILog::Log(LEVEL_SIDB_PREPARE_FINALIZE, "{SI_SQLT3_Connection::query} finalize (%s, %d)", __FILE__, __LINE__);
#endif //ALLOW_PERFORMANCE_ISSUES
            rc = sqlite3_finalize(pStmt);
    #ifdef ALLOW_PERFORMANCE_ISSUES
                    SILog::Log(LEVEL_SIDB_SQLITE, "{SI_SQLT3_Connection::query} sqlite_finalize (result=%d)\n", rc);
    #endif //ALLOW_PERFORMANCE_ISSUES

        }
        else //writing
        {
    #ifdef ALLOW_PERFORMANCE_ISSUES
                    CString sClassName(pObjectDataset.getRuntimeClass().m_lpszClassName);
                    SILog::Log(LEVEL_SIDB, "{SI_SQLT3_Connection::query} Entering write mode for collection %s\n", Unicode2UTF8(sClassName).c_str());
    #endif //ALLOW_PERFORMANCE_ISSUES
            SQLObjectList objects_to_update;        
            if (pObjectDataset.getObjectsToUpdate(objects_to_update) > 0)
            {
            SQLObjectList::iterator it;
            for (it = objects_to_update.begin(); it != objects_to_update.end(); it++)
            {
                SISQLObject* pSQLObject = *it;
                CString sQuery;

                iParamsCount = pSQLObject->getUpdateQuery(sQuery, bWrite, false);
                utf8Query = Unicode2UTF8(sQuery);                        
    #ifdef ALLOW_PERFORMANCE_ISSUES
                SILog::Log(LEVEL_SIDB, "{SI_SQLT3_Connection::query} iParamsCount:%d, sQuery:%s", iParamsCount, utf8Query.c_str());
    #endif //ALLOW_PERFORMANCE_ISSUES
                do
                {
#ifdef ALLOW_PERFORMANCE_ISSUES
            SILog::Log(LEVEL_SIDB_PREPARE_FINALIZE, "{SI_SQLT3_Connection::query} prepare (%s, %d)", __FILE__, __LINE__);
#endif //ALLOW_PERFORMANCE_ISSUES
                rc = sqlite3_prepare(m_pCnnHandle, utf8Query.c_str(), (int)utf8Query.size(), &pStmt, NULL);
                bFinalized = false;
                if (SQLITE_OK != rc)
                {
                    throw SIDBException(m_pCnnHandle);
                }

                writeRecord(pStmt, pSQLObject, iParamsCount);

                //running the query
                rc = sqlite3_step(pStmt);
                if (rc == SQLITE_ROW)
                    throw SIDBException(_T("Non select query returned row(s)."));

                bFinalized = true;
#ifdef ALLOW_PERFORMANCE_ISSUES
            SILog::Log(LEVEL_SIDB_PREPARE_FINALIZE, "{SI_SQLT3_Connection::query} finalize (%s, %d)", __FILE__, __LINE__);
#endif //ALLOW_PERFORMANCE_ISSUES
                rc = sqlite3_finalize(pStmt);



                }
                while (rc == SQLITE_SCHEMA);

                iObjectsCount++;

                //notify listeners about objects count change
                std::list<fw::core::GenericListener*>::iterator listenerIt;
                for (listenerIt = m_Listeners.begin(); listenerIt != m_Listeners.end(); listenerIt++)
                ((SI_SQLT3_Connection_Listener*)(*listenerIt))->onUpdatedItemsCountChange(iObjectsCount);

            }

            //commit the transaction
            //if (SQLITE_OK != sqlite3_exec(m_pCnnHandle, sqlCOMMIT.c_str(), NULL, NULL, NULL))
            //{
            //  throw SIDBException(_T("Writing operation failed."));
            //}

            //writing operation completed successfully, change state 
            //of all objects written
            for (it = objects_to_update.begin(); it != objects_to_update.end(); it++)
            {
                if (SISQLObject::STATE_DELETE != (*it)->getState())
                (*it)->setState(SISQLObject::STATE_OK);            
            }

            //allow dataset to destroy "deleted" objects
            destroyDeletedObjects(&pObjectDataset);

            }
            
        }
    /*
        std::string sQuery = Unicode2UTF8(sSQLQueries);
        char szErrorMsg[1024];
        char* p = szErrorMsg;

        sqlite3_exec(m_pCnnHandle, sQuery.c_str(), QueryCallbackRowDataSet, (void*)&pObjectDataset, &p);
    */
    }
    else
    {
        throw SIDBException(_T("Database is not open."));
    }
  }
  catch (SIDBException& ex)
  {
      if (false == bFinalized)
      {
#ifdef ALLOW_PERFORMANCE_ISSUES
            SILog::Log(LEVEL_SIDB_PREPARE_FINALIZE, "{SI_SQLT3_Connection::query} finalize (%s, %d)", __FILE__, __LINE__);
#endif //ALLOW_PERFORMANCE_ISSUES
          sqlite3_finalize(pStmt);
      }
      
      throw ex;

  }

  return iObjectsCount;

}


int SI_SQLT3_Connection::QueryCallbackRowDataSet(void* pParm, int pColCnt, char** pRowVals ,char** pColNames)
{
  //recover pointer to dataset
  SISQLRowDataSet* pRowDataset = (SISQLRowDataSet*)pParm;
  if (pRowDataset)
  {
    SISQLRow oSingleRow;
    for (int i = 0; i < pColCnt; i++)
    {      
      oSingleRow.addValue(pColNames[i], pRowVals[0]);      
    }
    pRowDataset->append(oSingleRow);

  }
  else return 1; //not sure what to return in such case !!!

  return 0;
}


int SI_SQLT3_Connection::QueryCallbackObjectDataSet(void* pParm, int pColCnt, char** pRowVals ,char** pColNames)
{

  int result = 0;

  //recover pointer to dataset
  SISQLObjectDataSet* pObjectDataSet = (SISQLObjectDataSet*)pParm;
  if (pObjectDataSet)
  {
    //produce a row object 
    SISQLRow oSingleRow;
    for (int i = 0; i < pColCnt; i++)
    {      
      oSingleRow.addValue(pColNames[i], pRowVals[1]);      
    }

    //produce the SQLObject 
    ASSERT(pObjectDataSet->getRuntimeClass().IsDerivedFrom(RUNTIME_CLASS(SISQLObject)));

    CObject* pObject = pObjectDataSet->getRuntimeClass().CreateObject();
    if (pObject)
    {
      SISQLObject* pSISQLObject = (SISQLObject*)pObject;
      if (pSISQLObject)
      {

      }
      else result = 1;
    }
    else result = 1;

    delete pObject;
    pObject = NULL;

    
  }
  else
    result = 1; //TODO: not sure what to return in such case;

  return result;
}



int SI_SQLT3_Connection::countItemsToLoad(SISQLObjectDataSet& pObjectDataset)
{

  //CObject* pObject = pObjectDataset.createObject();
  std::auto_ptr<CObject> pObject(pObjectDataset.createObject());
  if (pObject.get())
  {
    
    SISQLObject* pSQLObject = (SISQLObject*)pObject.get();
    if (NULL != pSQLObject)
    {
      CString sCountQuery;
      pSQLObject->getUpdateQuery(sCountQuery, false, true);

      if (sCountQuery.GetLength())
      {
        SISQLRowDataSet oDS(sCountQuery);
        query(oDS);

        if (false == oDS.isEmpty())
        {
          //assuming that count query returns only one row and one value in that row
          return oDS.getRowList().front().getValueAsInt(0);
        }
        
      }
    }

    //delete pObject;
    //pObject = NULL;

  }

  return 0;
}



void SI_SQLT3_Connection::beginTransaction()
{

  if (m_pCnnHandle)
  {

    char szErrorMsg[1024];
    char* p = szErrorMsg;

    int iResult = sqlite3_exec(m_pCnnHandle, sqlBEGIN_TRANSACTION.c_str(), NULL, NULL, &p);
    if (SQLITE_OK != iResult)
    { 
      CString sErr = UTF82Unicode(p);
      CString sMsg;
      if (sErr.IsEmpty())
        sMsg.Format(_T("Cannot start transaction."));
      else
        sMsg.Format(_T("Cannot start transaction (%s)."), sErr);
        
      throw SIDBException(sMsg);
    }
  }
  else
    throw sidb::SIDBException(_T("Connection is not opened."));



}


void SI_SQLT3_Connection::endTransaction(bool bCommit)
{

  if (m_pCnnHandle)
  {
    if (SQLITE_OK != sqlite3_exec(m_pCnnHandle, bCommit ? sqlCOMMIT.c_str() : sqlROLLBACK.c_str(), NULL, NULL, NULL))
    {

        int iErrorCode = sqlite3_errcode(m_pCnnHandle);
        const char* pErrorMsg = sqlite3_errmsg(m_pCnnHandle);  

      throw SIDBException(_T("Commit transaction failed."));
    }
  }
  else
    throw sidb::SIDBException(_T("Connection is not opened."));

}


bool SI_SQLT3_Connection::tableExists(const char* pTableName)
{
    bool bResult = false;
    if (m_pCnnHandle)
    {
        sqlite3_stmt* pStmt = NULL;

        std::string sQuery("SELECT name FROM sqlite_master WHERE type='table' AND name='");
        sQuery.append(pTableName);
        sQuery.append("';");
#ifdef ALLOW_PERFORMANCE_ISSUES
            SILog::Log(LEVEL_SIDB_PREPARE_FINALIZE, "{SI_SQLT3_Connection::query} prepare (%s, %d)", __FILE__, __LINE__);
#endif //ALLOW_PERFORMANCE_ISSUES
        int rc = sqlite3_prepare(m_pCnnHandle, sQuery.c_str(), (int)sQuery.size(), &pStmt, NULL);
#ifdef ALLOW_PERFORMANCE_ISSUES
                SILog::Log(LEVEL_SIDB_SQLITE, "{SI_SQLT3_Connection::tableExists} sqlite_prepare(result=%d)\n", rc);
#endif //ALLOW_PERFORMANCE_ISSUES
        if (SQLITE_OK != rc)
          throw SIDBException(m_pCnnHandle);

        rc = sqlite3_step(pStmt);
        if (SQLITE_ROW == rc)
        {
            //checking the first column in the first row
            int iCol = 0;
            int iColType = sqlite3_column_type(pStmt, iCol);
            const char* pColName = sqlite3_column_name(pStmt, iCol);

			if (SQLITE_TEXT == iColType && (0 == strcmp(pColName, "name")))
			{
				//it could happen that names have different case...
				const char* pColValue = (const char*)sqlite3_column_text(pStmt, iCol);
				bResult = (0 == strcmp(pColValue, pTableName));				
			}
        }
#ifdef ALLOW_PERFORMANCE_ISSUES
            SILog::Log(LEVEL_SIDB_PREPARE_FINALIZE, "{SI_SQLT3_Connection::query} finalize (%s, %d)", __FILE__, __LINE__);
#endif //ALLOW_PERFORMANCE_ISSUES
        rc = sqlite3_finalize(pStmt);
#ifdef ALLOW_PERFORMANCE_ISSUES
                SILog::Log(LEVEL_SIDB_SQLITE, "{SI_SQLT3_Connection::tableExists} sqlite_finalize (result=%d)\n", rc);
#endif //ALLOW_PERFORMANCE_ISSUES
    }

    return bResult;

}


}; //namespace