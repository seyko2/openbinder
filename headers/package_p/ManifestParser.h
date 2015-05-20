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

#ifndef _PACKAGE_MANIFESTPARSER_H
#define _PACKAGE_MANIFESTPARSER_H

#include <support/Package.h>
#include <support/String.h>
#include <support/ByteStream.h>


#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace package {
#endif


class SManifestParser : public SLightAtom
{
public:
	//!	Parses an IByteInput and calls the callbacks on a parser
	static status_t	ParseManifest(const SString& filename, const sptr<SManifestParser>& parser, const sptr<IByteInput>& stream, const SPackage& resources, const SString& package);

	//!	Parses an IByteInput that is at the beginning of a package file, looking for an
	//	embedded manifest file, and calls the callbacks on a parser
	static status_t ParseManifestFromPackageFile(const SString& filename, const sptr<SManifestParser>& parser, const sptr<IByteInput>& stream, const SPackage& resources, const SString& package);
	
	//! Callback: A protein plugin was declared
	virtual void OnDeclareAddon(const SValue& addonInfo) = 0;

	//! Callback: An application was declared
	virtual void OnDeclareApplication(const SValue& appInfo) = 0;
	
	//! Callback: A new component was declared
	virtual void OnDeclareComponent(const SValue& componentInfo) = 0;
};


#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::package
#endif

#endif // _PACKAGE_MANIFESTPARSER_H
