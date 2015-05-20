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

#include <package_p/ManifestParser.h>
#include <support/StdIO.h>
#include <support/IByteStream.h>

#include <xml/DataSource.h>
#include <xml/XML2ValueParser.h>
#include <xml/Parser.h>

#include <linux/cramfs_fs.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace package {

using namespace palmos::support;
using namespace palmos::xml;
#endif

B_CONST_STRING_VALUE_LARGE(key_package, "package", )

class BManifestParseCreator : public BCreator
{
public:
	BManifestParseCreator(const SString& filename, const sptr<SManifestParser>& parser, const SPackage& resources, const SString& package);
	
	virtual status_t OnStartTag(SString& name, SValue& attributes, sptr<BCreator>& newCreator);
	virtual status_t OnEndTag(SString& name);
	virtual status_t OnText(SString& data);
	virtual status_t Done();

protected:
	virtual ~BManifestParseCreator();

private:
	const sptr<SManifestParser> m_parser;
	const SPackage m_resources;
	SString m_data;
	enum {TOP=0, MANIFEST, COMPONENT, APPLICATION, ADDON, BLAH_PADDING=0xffffffff} m_state;
	SValue m_componentInfo;
	SString m_package;
	SString m_filename;
};

class BManifestParseComponent : public BCreator
{
public:
	BManifestParseComponent(const SString& filename, const SValue &attributes, SValue &componentInfo, const SPackage &resources);
	
	virtual status_t OnStartTag(SString& name, SValue& attributes, sptr<BCreator>& newCreator);
	virtual status_t OnEndTag(SString& name);
	virtual status_t OnText(SString& data);
	virtual status_t Done();
	
private:
	SValue &m_componentInfo;
	const SPackage m_resources;
	SString m_filename;
};

class BManifestParseApplication : public BCreator
{
public:
	BManifestParseApplication(const SString& filename, const SValue &attributes, SValue &appInfo, const SPackage& resources);
	
	virtual status_t OnStartTag(SString& name, SValue& attributes, sptr<BCreator>& newCreator);
	virtual status_t OnEndTag(SString& name);
	virtual status_t OnText(SString& data);
	virtual status_t Done();
	
private:
	SValue &m_appInfo;
	SValue m_intent;
	const SPackage m_resources;
	SString m_filename;
	bool m_haveIntent:1;
};

class BManifestParseAddon : public BCreator
{
public:
	BManifestParseAddon(const SString& filename, const SValue &attributes, SValue &componentInfo, const SPackage &resources);
	
	virtual status_t OnStartTag(SString& name, SValue& attributes, sptr<BCreator>& newCreator);
	virtual status_t OnEndTag(SString& name);
	virtual status_t OnText(SString& data);
	virtual status_t Done();
	
private:
	SString m_filename;
};

// ==================================================================================

BManifestParseCreator::BManifestParseCreator(const SString& filename, const sptr<SManifestParser>& parser, const SPackage& resources, const SString& package)
	:m_parser(parser),
	 m_resources(resources),
	 m_data(),
	 m_state(TOP),
	 m_componentInfo(),
	 m_package(package),
	 m_filename(filename)
{
}

BManifestParseCreator::~BManifestParseCreator()
{
	if (m_state == MANIFEST) {
		berr << m_filename << ": Manifest error:  <manifest> tag not correctly closed." << endl;
	}
}


status_t BManifestParseCreator::OnStartTag(SString& name, SValue& attributes, sptr<BCreator>& newCreator)
{
	switch (m_state)
	{
		case TOP:
		{
			if (name != "manifest") {
				berr << m_filename << ": Manifest error:  Require <manifest> tag at top." << endl;
				return B_BAD_VALUE;
			}
			m_state = MANIFEST;
		}
		break;
		case MANIFEST:
		{
			if (name == "component") {
				m_state = COMPONENT;
				attributes.JoinItem(key_package, SValue::String(m_package));
				newCreator = new BManifestParseComponent(m_filename, attributes, m_componentInfo, m_resources);
			}
			else if (name == "application") {
				m_state = APPLICATION;
				attributes.JoinItem(key_package, SValue::String(m_package));
				newCreator = new BManifestParseApplication(m_filename, attributes, m_componentInfo, m_resources);
			}
			else if (name == "addon")
			{
				m_state = ADDON;
				attributes.JoinItem(key_package, SValue::String(m_package));
				newCreator = new BManifestParseAddon(m_filename, attributes, m_componentInfo, m_resources);
			}
			else {
				berr << m_filename << ": Manifest error:  Expected <component> tag below <manifest>." << endl;
				return B_BAD_VALUE;
			}
		}
		break;
	}
	
	return B_OK;
}


status_t BManifestParseCreator::OnEndTag(SString& name)
{
	switch (m_state)
	{
		case TOP:
		{
		}
		case MANIFEST:
		{
			m_state = TOP;
			if (name != "manifest") {
				berr << m_filename << ": Manifest error:  <manifest> tag not correctly closed.  Found \"" << name << "\"" << endl;
			}
			break;
		}

		case COMPONENT:
		{
			m_state = MANIFEST;
			if (name == "component") {
				// bout << "Component Declared: " << m_componentInfo["local"].AsString() << endl;
				m_parser->OnDeclareComponent(m_componentInfo);
			} else {
				berr << m_filename << ": Manifest error:  <component> tag not correctly closed.  Found \"" << name << "\"" << endl;
			}
			break;
		}

		case APPLICATION:
		{
			m_state = MANIFEST;
			if (name == "application") {
				// bout << "Application Declared: " << m_componentInfo["name"].AsString() << endl;
				m_parser->OnDeclareApplication(m_componentInfo);
			} else {
				berr << m_filename << ": Manifest error:  <application> tag not correctly closed.  Found \"" << name << "\"" << endl;
			}
			break;
		}
		
		case ADDON:
		{
			m_state = MANIFEST;
			if (name == "addon")
			{
				m_parser->OnDeclareAddon(m_componentInfo);
			}
			else
			{
				berr << m_filename << ": Manifest error:  <addon> tag not correctly closed.  Found \"" << name << "\"" << endl;
			}
			break;
		}
		
		default:
		{
			break;
		}
	}
	m_data.Truncate(0);
	return B_OK;
}


status_t
BManifestParseCreator::OnText(SString& data)
{
	(void) data;
	return B_OK;
}


status_t
BManifestParseCreator::Done()
{
	return B_OK;
}


// ==================================================================================

BManifestParseComponent::BManifestParseComponent(const SString& filename, const SValue &attributes, SValue &componentInfo, const SPackage& resources)
	:m_componentInfo(componentInfo),
	 m_resources(resources),
	 m_filename(filename)
{
	m_componentInfo = attributes;
}


status_t BManifestParseComponent::OnStartTag(SString& name, SValue& attributes, sptr<BCreator>& newCreator)
{
	if (name == "value" || name == "property") {
		newCreator = new BXML2ValueCreator(m_componentInfo, attributes, m_resources);
		return B_OK;
	} else if (name == "interface") {
		m_componentInfo.JoinItem(SValue::String("interface"), attributes["name"]);
		return B_OK;
	} else if (name == "process") {
		m_componentInfo.JoinItem(SValue::String("process"), attributes);
		return B_OK;
	}
	
	berr << m_filename << ": Manifest error:  Require <value>, <property>, <process>, or <interface> inside <component>." << endl;
	return B_BAD_VALUE;
}


status_t BManifestParseComponent::OnEndTag(SString& name)
{
	(void) name;
	
	return B_OK;
}


status_t BManifestParseComponent::OnText(SString& data)
{
	(void) data;
	return B_OK;
}


status_t BManifestParseComponent::Done()
{
	return B_OK;
}

// ==================================================================================

BManifestParseApplication::BManifestParseApplication(const SString& filename, const SValue &attributes, SValue &componentInfo, const SPackage& resources)
	:m_appInfo(componentInfo),
	 m_resources(resources),
	 m_filename(filename),
	 m_haveIntent(false)
{
	m_appInfo = attributes;
}


status_t BManifestParseApplication::OnStartTag(SString& name, SValue& attributes, sptr<BCreator>& newCreator)
{
	SString appType = m_appInfo["type"].AsString();
	if (appType == "protein") {
		newCreator = new BXML2ValueCreator(m_appInfo, attributes, m_resources);
		return B_OK;
	}
	else if (appType == "rome") {
		if (!m_haveIntent && name == "intent") {
			// XXX create an intent parser
			m_haveIntent = true;
			return B_OK;
		}
		else {
			berr << m_filename << ": Manifest Error: unexpected tag inside a rome app: " << name << " " << attributes << endl;
			return B_BAD_VALUE;
		}
	}
	berr << m_filename << ": Manifest error: invalid type field in an application tag: " << m_appInfo << endl;
	return B_BAD_VALUE;
}


status_t BManifestParseApplication::OnEndTag(SString& /*name*/)
{
	return B_OK;
}


status_t BManifestParseApplication::OnText(SString& data)
{
	if (data.IsOnlyWhitespace()) {
		return B_OK;
	}
	berr << m_filename << ": Manifest error: invaild text inside an application tag: \"" << data << "\"" << endl;
	return B_BAD_VALUE;
}


status_t BManifestParseApplication::Done()
{
	return B_OK;
}

// ==================================================================================

BManifestParseAddon::BManifestParseAddon(const SString& filename, const SValue &attributes, SValue &componentInfo, const SPackage& resources)
	:	m_filename(filename)
{
	componentInfo = attributes;
}


status_t BManifestParseAddon::OnStartTag(SString& name, SValue& attributes, sptr<BCreator>& newCreator)
{
	berr << m_filename << ": Manifest Error: unexpected tag inside a addon: " << name << " " << attributes << endl;
	return B_BAD_VALUE;
}


status_t BManifestParseAddon::OnEndTag(SString& name)
{
	(void) name;
	return B_OK;
}


status_t BManifestParseAddon::OnText(SString& data)
{
	(void) data;
	return B_OK;
}


status_t BManifestParseAddon::Done()
{
	return B_OK;
}

// ==================================================================================

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::osp

namespace palmos {
namespace package {

using namespace palmos::xml;
using namespace palmos::osp;
#endif


status_t
SManifestParser::ParseManifest(const SString& filename, const sptr<SManifestParser>& parser, const sptr<IByteInput>& stream, const SPackage& resources, const SString& package)
{
	//bout << "=============================================" << endl;
	//bout << "Parsing Manifest file: " << filename << endl;
	BXMLIByteInputSource source(stream);
	sptr<BManifestParseCreator> creator = new BManifestParseCreator(filename, parser, resources, package);
	return ParseXML(creator.ptr(), &source, 0);
}


/*!	Simple adapter class for BXMLIByteInputSource which will scan the byte-stream and
	cut it off early if it sees a NUL character, or if goes longer than a fixed length.
	The NUL termination is only safe on UTF-8 streams, not UTF-16 or higher.

	We need this adapter for reading the manifest stuffed into the 'image' block of a
	cramfs filesystem image, which is within a larger file (hence the need to cap), and
	may have up to 3 NUL padding bytes placed at the end. 
*/

class CappedXMLIByteInputSource : public BXMLIByteInputSource
{
public:
	CappedXMLIByteInputSource(const sptr<IByteInput>& data, size_t cap)
		:	BXMLIByteInputSource(data), 
			m_cap(cap)
	{
	}
	
	virtual ~CappedXMLIByteInputSource()
	{
	}
	
	virtual status_t GetNextBuffer(size_t * size, uint8_t ** data, int * done)
	{
		status_t result = BXMLIByteInputSource::GetNextBuffer(size, data, done);
		if (result)
			return result;

		if (*size > m_cap) {
			*size = m_cap;
		}
		
		if (*size > 0) {
			void * v;
			if (v = memchr(*data, '\0', *size)) {
				*size = (uint8_t*)v - *data;
				*done = 1;
			}
		}
		
		if (*size == 0 && !*done)
			*done = 1;
		 
		m_cap -= *size;
		return result;
	}

private:
	size_t m_cap;
};

status_t
SManifestParser::ParseManifestFromPackageFile(const SString& filename, const sptr<SManifestParser>& parser, const sptr<IByteInput>& stream, const SPackage& resources, const SString& package)
{
	struct cramfs_super super;

	if (stream->Read((void*)&super, sizeof(super)) < sizeof(super))
	{
		return B_ERROR;
	}

	if (super.magic != CRAMFS_MAGIC || (super.flags & CRAMFS_FLAG_SHIFTED_ROOT_OFFSET) == 0)
	{
		return B_BAD_TYPE;
	}

	size_t manifest_length = (super.root.offset << 2) - sizeof(super);

	bout << "=============================================" << endl;
	bout << "Parsing Manifest file from cramfs image: " << filename << endl;
	CappedXMLIByteInputSource source(stream, manifest_length);
	sptr<BManifestParseCreator> creator = new BManifestParseCreator(filename, parser, resources, package);

	return ParseXML(creator.ptr(), &source, 0);
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif
