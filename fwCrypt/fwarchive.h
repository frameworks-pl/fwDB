#ifndef SIARCHIVE_H_INCLUDED
#define SIARCHIVE_H_INCLUDED

#include "fwsha256.h"
#include "fwaesencdec.h"


namespace fw 
{

    namespace crypt {
    
    /**
      @brief SIArchive class with implemented
      AES encryption/decryption.
      This class provides only two operators 
      (for CString and int), both of them encrypt
      values using AES algorithm from sicrypt library.
      The class also provides Read and Write functions
      that (so far behave in the same way as their
      CArchive counterparts).
    */
    
    class Archive : protected CArchive {
    
      public:
        using CArchive::IsStoring;
        using CArchive::IsLoading;
        using CArchive::Close;
        using CArchive::Flush;
        using CArchive::Read;
        using CArchive::Write;
        using CArchive::store;
        using CArchive::load;
    
    
        /** @brief sets the password */
        void setPassword(BYTE* pBuffer, int pLength);
    
        Archive( CFile* pFile, UINT nMode, int nBufSize = 4096, void* lpBuf = NULL );
        ~Archive();
    
        fw::core::ByteBuffer encryptChunk(const CString& pChunk);	
        CString decryptChunk(const fw::core::ByteBuffer& pChunk);
    
        void encryptBuffer(const fw::core::ByteBuffer& pSrcBuffer, fw::core::ByteBuffer& pTrgtBuffer);
        void decryptBuffer(const fw::core::ByteBuffer& pSrcBuffer, fw::core::ByteBuffer& pTrgtBuffer);
    
        /** Overridden opeartor>> for CString which uses encryption */
        Archive& operator>>(CString& pValue);
        Archive& operator<<(CString pValue);
    
        /** Overriden operator>> for int which uses encryption */
        Archive& operator>>(int& pValue);
        Archive& operator<<(int pValue);
    
    
    
      protected:
    
        /**
          @brief Encryptor/decryptor used by the archive to store load data
        */
        AESEncDec* mAES;
    
        /**
          @brief Sets the empty password for the archive,
          that is SHA156 generated from the empty string
        */
        void setEmptyPassword();
    
    
    
    }; //class
    
    } //namespace crypto
    
} //namspace fw



#endif //SIARCHIVE_H_INCLUDED
