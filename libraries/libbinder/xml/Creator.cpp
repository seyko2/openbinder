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

#include <xml/Parser.h>
#if BUILD_TYPE == BUILD_TYPE_DEBUG
#include <support/StdIO.h>
#endif

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace xml {
using namespace palmos::support;
#endif

class BCreatorParseContext : public BXMLParseContext
{
public:
						BCreatorParseContext(sptr<BCreator> creator);
	virtual				~BCreatorParseContext();
	
	virtual status_t	OnStartTag(				SString		& name,
												SValue		& attributes		);
									
	virtual status_t	OnEndTag(				SString		& name				);
	
	virtual status_t	OnTextData(				const char	* data,
												int32_t		size				);
	
	virtual status_t	OnCData(				const char	* data,
												int32_t		size				);
	
	virtual status_t	OnComment(				const char	* data,
												int32_t		size				);
	
	virtual status_t	OnProcessingInstruction(SString		& target,
												SString		& data				);
	
				
#if BUILD_TYPE == BUILD_TYPE_DEBUG
	virtual status_t	OnError(status_t error, bool fatal, int32_t debugLineNo,
									uint32_t code = 0, void * data = NULL);
#endif

private:
	status_t	do_text(const sptr<BCreator>& top);
	
	SVector<sptr<BCreator> >	m_creators;
	SString					m_textData;
	enum {TEXT, COMMENT, BLAH_PADDING=0xffffffff}	m_textType;
};




BCreatorParseContext::BCreatorParseContext(sptr<BCreator> creator)
	: m_textType(BLAH_PADDING) // VALGRIND
{
	m_creators.Push(creator);
}


BCreatorParseContext::~BCreatorParseContext()
{
}


status_t
BCreatorParseContext::OnStartTag(	SString		& name,
									SValue		& attributes		)
{
	status_t err;
	sptr<BCreator> top = m_creators.Top();
	err = do_text(top);
	if (err != B_OK) return err;
	sptr<BCreator> newCreator;
	err = top->OnStartTag(name, attributes, newCreator);
	if (err != B_OK) return err;
	if (newCreator == NULL) newCreator = top;
	m_creators.Push(newCreator);
	return B_OK;
}

									
status_t
BCreatorParseContext::OnEndTag(		SString		& name				)
{
	status_t err;
	sptr<BCreator> top = m_creators.Top();
	err = do_text(top);
	if (err != B_OK) return err;
	top->Done();
	m_creators.Pop();
	err = m_creators.Top()->OnEndTag(name);
	if (err != B_OK) return err;
	return B_OK;
}
	
status_t
BCreatorParseContext::OnTextData(	const char	* data,
									int32_t		size				)
{
	status_t err;
	if (m_textType != TEXT) {
		err = do_text(m_creators.Top());
		if (err != B_OK) return err;
	}
	m_textData.Append(data, size);
	m_textType = TEXT;
	return B_OK;
}
	
status_t
BCreatorParseContext::OnCData(		const char	* data,
									int32_t		size				)
{
	status_t err;
	if (m_textType != TEXT) {
		err = do_text(m_creators.Top());
		if (err != B_OK) return err;
	}
	m_textData.Append(data, size);
	m_textType = TEXT;
	return B_OK;
}


status_t
BCreatorParseContext::OnComment(	const char	* data,
									int32_t		size				)
{
	status_t err;
	if (m_textType != COMMENT) {
		err = do_text(m_creators.Top());
		if (err != B_OK) return err;
	}
	m_textData.Append(data, size);
	m_textType = COMMENT;
	return B_OK;
}


status_t
BCreatorParseContext::OnProcessingInstruction(SString		& target,
												SString		& data				)
{
	return m_creators.Top()->OnProcessingInstruction(target, data);
}

#if BUILD_TYPE == BUILD_TYPE_DEBUG
status_t
BCreatorParseContext::OnError(status_t error, bool fatal, int32_t debugLineNo, uint32_t code, void * data)
{
	bout << "XML parse error: " << (fatal ? "fatal " : "non-fatal ") << (void*)error << " at ParseXML.cpp:" << debugLineNo << endl;
	bout << "    Look near line " << line << ", column " << column << " in the XML source." << endl;
	return error;
}
#endif

status_t
BCreatorParseContext::do_text(const sptr<BCreator>& top)
{
	if (m_textData.Length() > 0) {
		status_t err = B_ERROR;
		switch (m_textType)
		{
			case TEXT:
				err = top->OnText(m_textData);
				break;
			case COMMENT:
				err = top->OnComment(m_textData);
				break;
		}
		m_textData.Truncate(0);
		return err;
	}
	return B_OK;
}


status_t
ParseXML(const sptr<BCreator>& creator, BXMLDataSource *data, uint32_t flags)
{
	BCreatorParseContext context(creator);
	return ParseXML(&context, data, flags);
}



// Default implementations...
BCreator::BCreator()
{
}

BCreator::~BCreator()
{
}

status_t
BCreator::OnStartTag(			SString			& /*name*/,
								SValue			& /*attributes*/,
								sptr<BCreator>	& /*newCreator*/	)
{
	return B_OK;
}

									
status_t
BCreator::OnEndTag(				SString		& /*name*/				)
{
	return B_OK;
}

	
status_t
BCreator::OnText(				SString		& /*data*/				)
{
	return B_OK;
}

	
status_t
BCreator::OnComment(			SString		& /*data*/				)
{
	return B_OK;
}

	
status_t
BCreator::OnProcessingInstruction(	SString		& /*target*/,
									SString		& /*data*/			)
{
	return B_OK;
}

status_t
BCreator::Done()
{
	return B_OK;
}


#if _SUPPORTS_NAMESPACE
}; // namespace xml
}; // namespace palmos
#endif
