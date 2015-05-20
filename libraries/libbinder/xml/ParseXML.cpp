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



#include <xml_p/XMLParserCore.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace xml {	
#endif


// Seems like we should probably make this configurable
#define PARSE_BUFFER_SIZE	4096

// Class to encapsulate parsing m_state.
class _does_the_parsing_yo
	: private XMLParserCore
{
public:
	_does_the_parsing_yo(BXMLDataSource * data, BXMLParseContext * context, bool dtdOnly, uint32_t flags, size_t bufferSize = PARSE_BUFFER_SIZE);
	~_does_the_parsing_yo(void);
status_t _do_the_parsing_yo_();


	// Members
private:
	size_t m_realBuffSize;
	BXMLDataSource * m_dataSource;
	uint8_t	* m_realBuff;
};

_does_the_parsing_yo::_does_the_parsing_yo(BXMLDataSource * data, BXMLParseContext * context, bool dtdOnly, uint32_t flags, size_t bufferSize)
: XMLParserCore(context, dtdOnly, flags)
, m_dataSource(data)
, m_realBuff(0)
, m_realBuffSize(bufferSize)
{
	m_realBuff = (uint8_t *) malloc(m_realBuffSize);
};

_does_the_parsing_yo::~_does_the_parsing_yo(void)
{
	if(m_realBuff)
	{
		free(m_realBuff);	
		m_realBuff = NULL;
	}
};



// =====================================================================
status_t _does_the_parsing_yo::_do_the_parsing_yo_()
{
	// Initialize state of parser.
	status_t err = B_OK;
	uint8_t	* buff = m_realBuff;
	size_t	buffSize = 0;
	bool errorExit = false;
	int		done = false;

	err = ProcessBegin();
	if (B_OK != err)
	{
		m_context->OnError(err, true, __LINE__);
		errorExit = true;
	}
	
	while (!done && !errorExit)
	{

		buffSize = m_realBuffSize;
		err = m_dataSource->GetNextBuffer(&buffSize, &buff, &done);
		if (B_OK != err)
		{
			m_context->OnError(err, true, __LINE__);
			errorExit = true;
		}
		else
		{
			err = ProcessInputBuffer(buffSize, buff, errorExit);
		}
	}

	err = ProcessEnd(done != 0);
	
	return err;
}


// =====================================================================
//            PUBLIC FUNCTIONS
// =====================================================================


// =====================================================================
status_t
ParseXML(BXMLParseContext *context, BXMLDataSource *data, uint32_t flags)
{
	// Generate appropriate flags.
	uint32_t realFlags = flags | B_XML_HANDLE_ATTRIBUTE_ENTITIES | B_XML_HANDLE_CONTENT_ENTITIES;

	// Create parser.
	_does_the_parsing_yo parser(data, context, false, realFlags);

	// Call the parser.
	return parser._does_the_parsing_yo::_do_the_parsing_yo_();
}


#if _SUPPORTS_NAMESPACE
}; // namespace xml
}; // namespace palmos
#endif
