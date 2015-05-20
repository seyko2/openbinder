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

#ifndef _DMPROVIDER_SCHEMADATABASENODE_H
#define _DMPROVIDER_SCHEMADATABASENODE_H

/*!	@file dmprovider/SchemaDatabaseNode.h
	@ingroup CoreDataManagerProvider
	@brief INode representing a schema database in the Data Manager,
		which has one or more tables.
*/

#include <dmprovider/SchemaTableNode.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace dmprovider {
using namespace palmos::storage;
#endif

/*!	@addtogroup CoreDataManagerProvider
	@ingroup Core
	@brief Implementation of @ref CoreSupportDataModel for Data Manager schema databases.

	This is a collection of classes for implementing the @ref BinderDataModel interfaces
	on top of Data Manager schema databases.  This make use of various lower-level
	convenience classes in the @ref CoreSupportDataModel APIs.
	
	@note These classes are not a part of the OpenBinder build, but included
	as an example for other similar implementations.
	@{
*/

// ==========================================================================
// ==========================================================================

//!	INode providing access to a particular Data Manager schema database.
/*!	The schema database must have one or more tables, which are accessed
	as sub-nodes from this one.  You can also create iterators directly
	from this node by specifying the desired table in the iterator
	arguments.

	This class understands one constructor argument (the @a args SValue,
	which is typically passed from InstantiateComponent):

	- "os:refPath": The absolute path to this object in the namespace,
	  to be returned by IReferable::Reference().
	
	@note These classes are not a part of the OpenBinder build, but included
	as an example for other similar implementations.

	@nosubgrouping
*/
class BSchemaDatabaseNode : public BIndexedDataNode, public BnReferable
{
public:
	// --------------------------------------------------------------
	/*!	@name Bookkeeping
		Creation, destruction, locking, etc. */
	//@{
			//!	Call-back function t create a database when it is missing.
	typedef	void (*database_missing_func)(const sptr<BSchemaDatabaseNode> &table);

									BSchemaDatabaseNode(const SContext& context,
														const SString& dbname,
														uint32_t creator,
														DmOpenModeType mode = dmModeReadOnly,
														DbShareModeType share = dbShareReadWrite,
														database_missing_func missing = NULL,
														const SValue& args = B_UNDEFINED_VALUE); 
									BSchemaDatabaseNode(const SContext& context,
														const SDatabase &database,
														const SValue& args = B_UNDEFINED_VALUE);
protected:
	virtual							~BSchemaDatabaseNode();
			//!	Open and initialize the database.
	virtual	void					InitAtom();
public:
			//!	Disambiguate.
	inline	SContext				Context() { return BIndexedDataNode::Context(); }
			//!	Make BIndexedDataNode and IReferable accessible.
	virtual	SValue					Inspect(const sptr<IBinder>& caller, const SValue& which, uint32_t flags);
			//!	Returns initialization state of object, such as error code when opening database.
			status_t				StatusCheck();

	//@}

	// --------------------------------------------------------------
	/*!	@name Implementation
		Provide default implementation based on new capabilities. */
	//@{

			//!	Return number of tables found.
	virtual	size_t					CountEntriesLocked() const;
			//!	Return index of a table.
	virtual	ssize_t					EntryIndexOfLocked(const SString& entry) const;
			//!	Return name of a table.
	virtual	SString					EntryNameAtLocked(size_t index) const;
			//!	Retrieve a table at an index.
	virtual	SValue					ValueAtLocked(size_t index) const;
			//!	Return B_UNSUPPORTED, can't change tables.
	virtual	status_t				StoreValueAtLocked(size_t index, const SValue& value);

			//!	Return the table object at a particular index.
			sptr<BSchemaTableNode>	TableAtLocked(size_t index) const;
			//!	Return the table object for a particular name.
			sptr<BSchemaTableNode>	TableForLocked(const SString& name) const;

			//!	Return the underlying opened SDatabase object.
			SDatabase				DatabaseLocked() const;

			//!	Create and return a new BSchemaTableNode.
			/*!	This is called for each table in the database.  The default implementation
				simply instantiates a BSchemaTableNode with the given arguments (and Context(),
				Database()). */
	virtual	sptr<BSchemaTableNode>	NewSchemaTableNodeLocked(const SString& table, const SString& refPath);

	//@}

	// --------------------------------------------------------------
	/*! @name IReferable Implementation */
	//@{

	virtual	SString					Reference() const;

	//@}

private:
									BSchemaDatabaseNode(const BSchemaDatabaseNode&);
			BSchemaDatabaseNode&	operator=(const BSchemaDatabaseNode&);

			void					open_database();
			void					retrieve_tables();

	const	SString					m_dbname;
	const	uint32_t				m_creator;
	const	DmOpenModeType			m_mode;
	const	DbShareModeType			m_share;
	const	database_missing_func	m_dbMissing;
	const	SString					m_refPath;


			status_t				m_status;
			SDatabase				m_database;

			SVector<sptr<BSchemaTableNode> >	m_tables;
			SKeyedVector<SString, ssize_t>		m_tableNameToIndex;
};

// ==========================================================================
// ==========================================================================

/*!	@} */

#if _SUPPORTS_NAMESPACE
} } // namespace palmos::dmprovider
#endif

#endif // _DMPROVIDER_SCHEMADATABASENODE_H
