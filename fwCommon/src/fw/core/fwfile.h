#ifndef FILE_H_INCLUDED
#define FILE_H_INCLUDED

namespace fw
{
	namespace core
	{
		class File
		{
			public:
				File()  : m_bDirectory(false)
				{ 
					m_Size.LowPart = 0;
					m_Size.HighPart = 0;
				}

				File(const CString& sPath, bool bDirectory = false) : m_sPath(sPath), m_bDirectory(bDirectory)
				{ 
					m_Size.LowPart = 0;
					m_Size.HighPart = 0;
				}
				virtual ~File() { }

				const CString& getPath() const { return m_sPath;  }
				bool isDirectory() const { return m_bDirectory;  }

			protected:

				//Full path
				CString m_sPath;

				//is directory
				bool m_bDirectory;

				//file size LARGE_INTEGER
				LARGE_INTEGER m_Size;
		};


		class FileSorter
		{
			enum Columns
			{
				eName = 0x1,
				eDirectory = 0x2
			};

		};

	}
}




#endif // !FILE_H_INCLUDED