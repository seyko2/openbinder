/*
 * Copyright (c) 2005 Palmsource, Inc.
 * 
 * This software is licensed as described in the file LICENSE, which
 * you should have received as part of this distribution. The terms
 * are also available at http://www.openbinder.org/license.html.
 * 
 * This software consists of voluntary contributions made by many
 * individuals. For the exact contribution history, see the revision
 * history and logs, available at http://www.openbinder.org
 */

#include <support/KernelStreams.h>
#include <support/KeyedVector.h>
#include <support/String.h>
#include <support/StdIO.h>
#include <support/TextStream.h>
#include <xml/Parser.h>

#include <getopt.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>


class StringsParser : public BCreator
{
public:
    StringsParser(const SString& filename, SKeyedVector<SString, SKeyedVector<SString, SString>* >& pairs);

	virtual status_t OnStartTag(SString& name, SValue& attributes, sptr<BCreator>& newCreator);
	virtual status_t OnEndTag(SString& name);
	virtual status_t OnText(SString& data);
	virtual status_t Done();

protected:
    virtual ~StringsParser();

private:
	enum
	{ 
		TOP = 0, 
		STRINGS,
		STRING,
		KEY,
		VALUE
	};
	
	uint32_t m_state;
	SString m_filename;
	SKeyedVector<SString, SKeyedVector<SString, SString>* >& m_pairs;
	SString m_key;
	SString m_locale;
	SString m_value;
};


StringsParser::StringsParser(const SString& filename, SKeyedVector<SString, SKeyedVector<SString, SString>* >& pairs)
	:	m_filename(filename),
		m_pairs(pairs)
{
}

StringsParser::~StringsParser()
{
}

status_t StringsParser::OnStartTag(SString& name, SValue& attributes, sptr<BCreator>& newCreator)
{
    switch (m_state)
    {
		case TOP:
		{
			if (name != "strings")
			{
				berr << m_filename << ": Manifest error:  Require <manifest> tag at top." << endl;
				return B_BAD_VALUE;
			}
			m_state = STRINGS;
			break;
		}

		case STRINGS:
		{
			if (name == "string")
			{
				m_state = STRING;
			}
			else
			{
				berr << m_filename << ": Manifest error:  Expected <component> tag below <manifest>." << endl;
				return B_BAD_VALUE;
			}
			break;
		}
			
		case STRING:
		{
			if (name == "key")
			{
				m_state = KEY; 
			}
			else if (name == "value")
			{
				m_state = VALUE;
				m_locale = attributes["locale"].AsString();
				
				if (m_locale != "" && m_pairs.ValueFor(m_locale) == NULL)
				{
					m_pairs.AddItem(m_locale, new SKeyedVector<SString, SString>());
				}
				else if (m_locale == "")
				{
					berr << "makestrings: could not create vector for locale '" << m_locale << "'" << endl;
					berr << indent << attributes << dedent << endl;
				}
			}
			
		}
    }
	
	return B_OK;
}

status_t StringsParser::OnText(SString& data)
{
	switch (m_state)
	{
		case KEY:
		{
			m_key = data;
			break;
		}
			
		case VALUE:
		{
			m_value = data;
			break;
		}
	}
	return B_OK;
}

status_t StringsParser::OnEndTag(SString& name)
{
	switch (m_state)
	{			
		case VALUE:
		{
			m_pairs.ValueFor(m_locale)->AddItem(m_key, m_value);
		
			m_locale = "";
			m_value = "";
			m_state = STRING;
			break;
		}
			
		case KEY:
		{
			m_state = STRING;
			break;
		}
		
		case STRING:
		{
			m_key = "";
			m_state = STRINGS;
			break;
		}
	}

	return B_OK;
}

status_t StringsParser::Done()
{
	return B_OK;
}


int usage()
{
	bout << "Usage: make_strings [OPTIONS] strings.xml " << endl << endl;
	bout << "Options:" << endl;
	bout << "  -O,  --output-dir=PATH   outputdir for generated files;" << endl;
	bout << "  -L,  --locale=LOCALE     only output localized string file for the given locale;" << endl;
	return 2;
}

status_t readargs(int argc, char ** argv, SString& outputdir, SVector<SString>& locales, SVector<SString>& files)
{
	static option long_options[] =
	{
		{"output-dir", 1, 0, 'O'},
		{"locale", 1, 0, 'L'},
		{0, 0, 0, 0}
	};
	
	char getopt_result;
	for (;;)
	{
		getopt_result=getopt_long(argc, argv, "O:L:h", long_options, NULL);
		
		if (getopt_result==-1) break;
		switch (getopt_result)
		{
			case 'O':
			{
				outputdir = optarg;
				break;
			}

			case 'L':
			{
				locales.AddItem(SString(optarg));
				break;
			}
			
			default:
			{
				return usage();
				break;
			}
		}
	}
	
	if (optind>=argc)
	{ 
		berr << argv[0] << ": no xml file specified" << endl;	
		return usage(); 
	}
	else
	{
		for (int ifile = optind ; ifile < argc ; ifile++)
		{
			files.AddItem(SString(argv[ifile]));		
		}
	}
		
	return B_OK;
}


status_t read_files(const SVector<SString>& files, SKeyedVector<SString, SKeyedVector<SString, SString>* >& pairs)
{
	const size_t COUNT = files.CountItems();
	for (size_t i = 0 ; i < COUNT ; i++)
	{
		int fd = open(files[i].String(), O_RDONLY);
		sptr<IByteInput> input = new BKernelIStr(fd);
		BXMLIByteInputSource source(input);
		sptr<StringsParser> creator = new StringsParser(files[i], pairs);
		status_t err = ParseXML(creator, &source, 0);
		
		if (err != B_OK)
		{
			return err;
		}
	}
	
	return B_OK;
}

status_t write_pairs(const SString& outputdir, const SString& locale, SKeyedVector<SString, SString>* pairs)
{
	SString path(outputdir);
	path.PathAppend(locale);

	status_t err = mkdir(path.String(), 0777);

	if (err != B_OK && errno != EEXIST)
	{
		berr << "makestrings: could not create directory '" << path << "' err = " << strerror(errno) << endl;
		return err;
	}	
	
	path.PathAppend("Strings.localized");
	int fd = open(path.String(), O_CREAT | O_RDWR, 0777);

	if (fd != -1)
	{
		const size_t COUNT = pairs->CountItems();
		
		write(fd, &COUNT, sizeof(uint32_t));

		const size_t ARRAY_SIZE = sizeof(uint32_t) * COUNT * 2;
		uint32_t* offsetArray = (uint32_t*)malloc(ARRAY_SIZE);
		
		uint32_t offset = sizeof(uint32_t) + ARRAY_SIZE;
		offsetArray[0] = offset;
		
		uint32_t offsetIndex = 1;
		
		for (size_t j = 0 ; j < COUNT ; j++)
		{
			offset += pairs->KeyAt(j).Length() + 1;
			offsetArray[offsetIndex] = offset;
			offsetIndex++;
		}

		for (size_t j = 0 ; j < COUNT ; j++)
		{
			offset += pairs->ValueAt(j).Length() + 1;
			offsetArray[offsetIndex] = offset;
			offsetIndex++;
		}

		write(fd, offsetArray, ARRAY_SIZE);

		for (size_t j = 0 ; j < COUNT ; j++)
		{
			write(fd, pairs->KeyAt(j).String(), pairs->KeyAt(j).Length() + 1);
		}

		for (size_t j = 0 ; j < COUNT ; j++)
		{
			write(fd, pairs->ValueAt(j).String(), pairs->ValueAt(j).Length() + 1);
		}
		
		off_t len = lseek(fd, 0, SEEK_END);
		lseek(fd, 0, SEEK_SET);
		char* buf = (char*)mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0);

		//printf("locale '%s' buf = %p, len = %d\n", locale.String(), buf, len);	
		if (buf != NULL)
		{	
			if (COUNT != *((uint32_t*)buf))
			{
				berr << "make_strings: key/pair count mismatch" << endl;
				return B_ERROR;
			}

			char* it = buf + sizeof(uint32_t);
			for (size_t j = 0 ; j < (COUNT * 2) ; j++)
			{
				//bout << "offset: " << *((uint32_t*)it) << " = '" << buf + *((uint32_t*)it) << "'" << endl;
				it += sizeof(uint32_t);
			}
			
			it = buf + sizeof(uint32_t);
			for (size_t j = 0 ; j < COUNT ; j++)
			{
				uint32_t keyOffset = *((uint32_t*)it);
				
				const char* key = buf + keyOffset;
				
				if (key != pairs->KeyAt(j))
				{
					berr << "make_strings: file key '" << key << "' != '" << pairs->KeyAt(j) << endl;
					return B_ERROR;
				}
				
				it += sizeof(uint32_t);
			}

			for (size_t j = 0 ; j < COUNT ; j++)
			{
				uint32_t valueOffset = *((uint32_t*)it);
				
				const char* value = buf + valueOffset;
				
				if (value != pairs->ValueAt(j))
				{
					berr << "make_strings: file key '" << value << "' != '" << pairs->ValueAt(j) << endl;
					return B_ERROR;
				}
				it += sizeof(uint32_t);
			}
		}

		munmap(buf, len);
		close(fd);
	}

	return B_OK;
}


status_t output_files(const SString& outputdir, const SKeyedVector<SString, SKeyedVector<SString, SString>* >& pairs, const SVector<SString>& locales)
{
	status_t err = B_OK;
	const size_t COUNT = locales.CountItems();
	
	if (COUNT > 0)
	{
		for (size_t i = 0 ; i < COUNT ; i++)
		{
			SString locale = locales.ItemAt(i);
			SKeyedVector<SString, SString>* vector = pairs.ValueFor(locale);
			
			if (vector != NULL)
			{
				err = write_pairs(outputdir, locale, vector);
				if (err != B_OK)
				{
					return err;
				}
			}
			else
			{
				berr << "Strings.xml did not contain strings for locale '" << locale << "'" << endl;
				return B_ERROR;
			}
		}

	}
	else
	{	
		const size_t LOCALES = pairs.CountItems();
		for (size_t i = 0 ; i < LOCALES ; i++)
		{
			err = write_pairs(outputdir, pairs.KeyAt(i), pairs.ValueAt(i));
			if (err != B_OK)
			{
				return err;
			}
		}
	}

	return B_OK;
}

int main(int argc, char** argv)
{
	SString outputdir;
	SVector<SString> locales;
	SVector<SString> files;
	
	status_t err = readargs(argc, argv, outputdir, locales, files);
	
	if (err == B_OK)
	{
		SKeyedVector<SString, SKeyedVector<SString, SString>* > pairs;
		
		err = read_files(files, pairs);
		
		if (err == B_OK)
		{
			err = output_files(outputdir, pairs, locales);
		}
	}

	return err;
}

