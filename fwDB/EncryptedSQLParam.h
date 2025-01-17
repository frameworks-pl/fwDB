#ifndef ENCRYPTEDSQLPARAM_H_INCLUDED
#define ENCRYPTEDSQLPARAM_H_INCLUDED

#include "SQLParam.h"
#include <fwaesencdec.h>

namespace fw
{
	namespace db
	{
		class FWDB_DLLEXPORT EncryptedSQLParam : public SQLParam
		{
			public:
				//constructs integer sql param
				EncryptedSQLParam(const CString& pColName, int* pIntValue, const fw::crypt::EncDec* pCipher);

				//constructs string sql param
				EncryptedSQLParam(const CString& pColName, CString* pStringValue, const fw::crypt::EncDec* pCipher);

				//constructs blob sql param
				EncryptedSQLParam(const CString& pColName, fw::core::ByteBuffer* pByteBuffer, const fw::crypt::EncDec* pCipher);

				//constructs blob pointer sql param
				EncryptedSQLParam(const CString& pColName, BLOBItem* pBLOBItem, const fw::crypt::EncDec* pCipher);

				bool getSQLFormattedValue(CString& pFormattedValue) const;
				void updateFromString(const std::string& pStringValue);

			protected:

				const fw::crypt::EncDec* m_pCipher;
		};

	}

}
#endif // !ENCRYPTEDSQLPARAM_H_INCLUDED
