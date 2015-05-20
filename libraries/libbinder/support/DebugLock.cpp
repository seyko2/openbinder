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

#include <support_p/DebugLock.h>

#if SUPPORTS_LOCK_DEBUG

#include <support/atomic.h>
#include <support/KeyedVector.h>
#include <support/CallStack.h>
#include <support/SortedVector.h>
#include <support/String.h>
#include <support/TLS.h>

#include <support_p/SupportMisc.h>

#include <stdio.h>
#include <stdlib.h>

#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace support {
#endif

struct link_info
{
	inline link_info()
	{
	}
	inline link_info(DebugLockNode* in_target, DebugLockNode* in_source)
		:	target(in_target), source(in_source), count(1)
	{
		thread = SysCurrentThread();
		if (LockDebugStackCrawls()) stack.Update(3);
	}
	inline link_info(const link_info& o)
		:	target(o.target), source(o.source),
			thread(o.thread), stack(o.stack),
			count(o.count)
	{
	}
	
	DebugLockNode*		target;		// next lock being acquired
	DebugLockNode*		source;		// last lock held while 'target' was acquired
	SysHandle			thread;		// thread that created this link
	SCallStack			stack;		// stack of this acquire of 'target'
	int32_t				count;		// number of times link has occurred.
};

B_IMPLEMENT_SIMPLE_TYPE_FUNCS(link_info);

struct block_links
{
	inline block_links()
		:	targets(link_info(NULL, NULL))
	{
	}
	
	SKeyedVector<DebugLockNode*, link_info>			targets;
	SSortedVector<DebugLockNode*>					sources;
};

// ----- Lock debugging level
//#pragma mark -

static int32_t gHasLockDebugLevel = 0;
int32_t gLockDebugLevel = -1;
bool gLockDebugStackCrawls = true;

int32_t LockDebugLevelSlow() {
	if (atomic_or(&gHasLockDebugLevel, 1) == 0) {
		const char* env = getenv("LOCK_DEBUG");
		if (env) {
			gLockDebugLevel = atoi(env);
			if (gLockDebugLevel < 0)
				gLockDebugLevel = 0;
		} else {
			// XXX During development, have lock debugging turned on
			// by default.
#if LIBBE_BOOTSTRAP
			gLockDebugLevel = 0;
#else
			gLockDebugLevel = 15;
#endif
		}
		env = getenv("LOCK_DEBUG_STACK_CRAWLS");
		if (env) {
			gLockDebugStackCrawls = atoi(env) ? true : false;
		} else {
			gLockDebugStackCrawls = true;
		}
		atomic_or(&gHasLockDebugLevel, 2);
		if (gLockDebugLevel > 0)
			printf("LOCK DEBUGGING ENABLED!  LOCK_DEBUG=%d  LOCK_DEBUG_STACK_CRAWLS=%d\n",
					gLockDebugLevel, gLockDebugStackCrawls);
	}
	while ((gHasLockDebugLevel&2) == 0) 
	   SysThreadDelay(B_MILLISECONDS(2), B_RELATIVE_TIMEOUT);
	return gLockDebugLevel;
}

// ----- Graph management and access protection
//#pragma mark -

static volatile int32_t gGraphLock = 0;
static SysHandle gGraphHolder = B_NO_INIT;
static int32_t gGraphNesting = 0;
static int32_t gHasGraphSem = 0;
static int32_t gLocksHeldTLS = -1;

static void AllocGraphLock();

static inline void LockGlobalGraph() {
	if ((gHasGraphSem&2) == 0) AllocGraphLock();
	const SysHandle me = SysCurrentThread();
	if (gGraphHolder != me) dbg_lock_gehnaphore(&gGraphLock);
	gGraphNesting++;
	gGraphHolder = me;
}

// These functions are used to keep track of all the locks held by
// a running thread.
static inline SSortedVector<DebugLockNode*>* LocksHeld()
{
	return static_cast<SSortedVector<DebugLockNode*>*>(tls_get(gLocksHeldTLS));
}
static inline void AddLockHeld(DebugLockNode* lock)
{
	SSortedVector<DebugLockNode*>* held = LocksHeld();
	if (!held) {
		held = new(B_SNS(std::) nothrow) SSortedVector<DebugLockNode*>;
		tls_set(gLocksHeldTLS, held);
	}
	if (held) held->AddItem(lock);
}
static inline void RemLockHeld(DebugLockNode* lock)
{
	SSortedVector<DebugLockNode*>* held = LocksHeld();
	if (held) {
		held->RemoveItemFor(lock);
		if (held->CountItems() == 0) {
			delete held;
			tls_set(gLocksHeldTLS, NULL);
		}
	}
}

// Look through the lock graph for a lock 'source' that is connected as
// a source to 'base'.  (That is, if in the past 'base' has been acquired
// while 'source' was held.)
static SVector<const link_info*>* FindSourceLock(	DebugLockNode* source,
													DebugLockNode* base)
{
	if (base->Links()) {
		SVector<const link_info*>* shortest = NULL;
		SVector<const link_info*>* v;
		int32_t i = base->Links()->sources.CountItems();
		while (i > 0) {
			--i;
			DebugLockNode* s = base->Links()->sources.ItemAt(i);
			if (s == source) {
				v = new(B_SNS(std::) nothrow) SVector<const link_info*>;
				ErrFatalErrorIf(!v, "Potential deadlock, but no memory to report it!");
			} else {
				v = FindSourceLock(source, s);
			}
			if (v) {
				v->AddItem(&(s->Links()->targets.ValueFor(base)));
				if (!shortest) {
					shortest = v;
				} else if (shortest->CountItems() > v->CountItems()) {
					delete shortest;
					shortest = v;
				} else {
					delete v;
				}
			}
		}
		
		return shortest;
	}
	return NULL;
}

static inline void UnlockGlobalGraph() {
	if (--gGraphNesting == 0) {
		gGraphHolder = B_NO_INIT;
		dbg_unlock_gehnaphore(&gGraphLock);
	}
}

void AssertNoLocks()
{
	if (gLocksHeldTLS < 0) return;

	SSortedVector<DebugLockNode*>* held = LocksHeld();
	if (held != NULL) {
		const int32_t N = held->CountItems();

		LockGlobalGraph();
		BStringIO* dirtyADSHack = new BStringIO();
		sptr<ITextOutput> msg = dirtyADSHack;
		msg << "Thread " << SysCurrentThread()
			<< " still has " << N << " locks held!" << endl << endl;
		for (int32_t i=0; i<N; i++) {
			msg << "Lock " << *held->ItemAt(i) << " acquired at:" << endl;
			msg->MoveIndent(1);
			held->ItemAt(i)->PrintStacksToStream(msg);
			msg->MoveIndent(-1);
			msg << endl;
		}
		UnlockGlobalGraph();

		printf("%s", dirtyADSHack->String());
		ErrFatalError(dirtyADSHack->String());
	}
}

static void ThreadExitCheck(void*) {
	AssertNoLocks();
}

static void AllocGraphLock() {
	if (atomic_or(&gHasGraphSem, 1) == 0) {
		dbg_init_gehnaphore(&gGraphLock);
		gLocksHeldTLS = tls_allocate();
		SysThreadExitCallbackID idontcare;
		SysThreadInstallExitCallback(ThreadExitCheck, NULL, &idontcare);
		atomic_or(&gHasGraphSem, 2);
	}
	while ((gHasGraphSem&2) == 0)
	   SysThreadDelay(B_MILLISECONDS(2), B_RELATIVE_TIMEOUT);
}

// ----- Lock contention tracking
//#pragma mark -

static volatile int32_t gContentionLock = 0;
static int32_t gHasContentionSem = 0;
static SVector<DebugLockNode*>* gHighContention = NULL;
static int32_t gMinContention = 1;
static SVector<DebugLockNode*>* gHighBlockCount = NULL;
static int32_t gMinBlockCount = 1;
static int32_t gNumContention = -1;
static int32_t gBlockCount = 0;			// total number of times we have had to block a thread
static nsecs_t gStartTime = 0;
static int32_t gContentionCount = 0;	// counter to print stats at regular interval

static void AllocContentionLock();

static inline bool LockContention() {
	if ((gHasContentionSem&2) == 0) AllocContentionLock();
	if (gNumContention <= 0) return false;
	dbg_lock_gehnaphore(&gContentionLock);
	return true;
}

typedef int32_t (*get_node_value_func)(DebugLockNode* who);
typedef void (*remove_node_func)(DebugLockNode* who);

static bool RegisterDebugNode(SVector<DebugLockNode*>& vec, DebugLockNode* who, get_node_value_func valGetter, remove_node_func nodeRemover)
{
	const int32_t C = valGetter(who);
	const int32_t N = vec.CountItems();
	DebugLockNode* node;
	
	// Determine where this item should go.
	ssize_t mid, low = 0, high = N-1;
	while (low <= high) {
		mid = (low + high)/2;
		node = vec.ItemAt(mid);
		if (node == NULL || C > valGetter(node)) {
			high = mid-1;
		} else if (C < valGetter(node)) {
			low = mid+1;
		} else {
			low = mid;
			break;
		}
	}

	if (low < N) {
		if (N >= gNumContention) {
			node = vec.ItemAt(N-1);
			//printf("Removing contention at %ld: %p\n", N-1, node);
			if (node) nodeRemover(node);
			vec.RemoveItemsAt(N-1);
		}
		vec.AddItemAt(who, low);
		//printf("Adding contention at %ld: %p (now have %ld)", low, who, gHighContention->CountItems());
		return true;
	}
	return false;
}

static bool UnregisterDebugNode(SVector<DebugLockNode*>& vec, DebugLockNode* who)
{
	const int32_t N = vec.CountItems();
	for (int32_t i=0; i<N; i++) {
		if (vec.ItemAt(i) == who) {
			vec.RemoveItemsAt(i);
			return true;
		}
	}
	return false;
}

static int32_t GetContentionValue(DebugLockNode* who)
{
	return who->MaxContention();
}

static void RemoveContentionNode(DebugLockNode* who)
{
	who->RemoveFromContention();
}

static bool RegisterContention(DebugLockNode* who)
{
	if (gHighContention) {
		const int32_t C = who->MaxContention();
		if (C >= gMinContention) {
			return RegisterDebugNode(*gHighContention, who, GetContentionValue, RemoveContentionNode);
		}
	}
	return false;
}

static bool UnregisterContention(DebugLockNode* who)
{
	if (gHighContention) {
		return UnregisterDebugNode(*gHighContention, who);
	}
	return false;
}

static int32_t GetBlockCountValue(DebugLockNode* who)
{
	return who->BlockCount();
}

static void RemoveBlockCountNode(DebugLockNode* who)
{
	who->RemoveFromBlockCount();
}

static void UpdateMinBlockCount()
{
	size_t N = gHighBlockCount->CountItems();
	DebugLockNode* node = N > 0 ? gHighBlockCount->ItemAt(N-1) : NULL;
	gMinBlockCount = node ? node->BlockCount() : 0;
}

static bool RegisterBlockCount(DebugLockNode* who)
{
	if (gHighBlockCount) {
		const int32_t C = who->BlockCount();
		if (C >= gMinBlockCount) {
			bool result = RegisterDebugNode(*gHighBlockCount, who, GetBlockCountValue, RemoveBlockCountNode);
			if (result) UpdateMinBlockCount();
			return result;
		}
	}
	return false;
}

static bool UnregisterBlockCount(DebugLockNode* who)
{
	if (gHighBlockCount) {
		bool result = UnregisterDebugNode(*gHighBlockCount, who);
		if (result) UpdateMinBlockCount();
		return result;
	}
	return false;
}

static inline void UnlockContention() {
	if (gNumContention <= 0) return;
	dbg_unlock_gehnaphore(&gContentionLock);
}

static void PrintContentionIfNeeded() {
	if (gNumContention > 0) {
		if ((atomic_add(&gContentionCount, 1)%1000) == 1) {
			BStringIO* dirtyADSHack = new BStringIO();
			sptr<ITextOutput> msg = dirtyADSHack;
			DebugLockNode::PrintContentionToStream(msg);
			printf("%s", dirtyADSHack->String());
		}
	}
}

static void AllocContentionLock() {
	if (atomic_or(&gHasContentionSem, 1) == 0) {
		dbg_init_gehnaphore(&gContentionLock);
#if TARGET_HOST == TARGET_HOST_PALMOS
		char envBuffer[128];
		const char* env = NULL;
		if (HostGetPreference("LOCK_CONTENTION", envBuffer)) env = envBuffer;
#else
		const char* env = getenv("LOCK_CONTENTION");
#endif
		//if (!env) env = "10";
		if (env) gNumContention = atoi(env);
		if (gNumContention > 0) {
			gHighContention = new(B_SNS(std::) nothrow) SVector<DebugLockNode*>;
			if (gHighContention) {
				gHighContention->SetCapacity(gNumContention);
				for (int32_t i=0; i<gNumContention; i++)
					gHighContention->AddItem(NULL);
			}
			gHighBlockCount = new(B_SNS(std::) nothrow) SVector<DebugLockNode*>;
			if (gHighBlockCount) {
				gHighBlockCount->SetCapacity(gNumContention);
				for (int32_t i=0; i<gNumContention; i++)
					gHighBlockCount->AddItem(NULL);
			}
		}
		if (gNumContention > 0)
			printf("LOCK CONTENTION PROFILING ENABLED!\n\tLOCK_CONTENTION=%ld\n",
					gNumContention);
		gStartTime = SysGetRunTime();
		atomic_or(&gHasContentionSem, 2);
	}
	while ((gHasContentionSem&2) == 0) 
	   SysThreadDelay(B_MILLISECONDS(2), B_RELATIVE_TIMEOUT);
}

// ----- Locking implementation
//#pragma mark -

DebugLockNode::DebugLockNode(	const char* type, const void* addr,
								const char* name, uint32_t flags)
	:	m_refs(1), m_type(type), m_addr(addr), m_name(name), m_globalFlags(flags),
		m_createStack(LockDebugLevel() > 5 ? new(B_SNS(std::) nothrow) SCallStack : NULL),
		m_createTime(SysGetRunTime()),
		m_links(LockDebugLevel() > 10 ? new(B_SNS(std::) nothrow) block_links : NULL),
		m_maxContention(0), m_inContention(false),
		m_blockCount(0), m_inBlockCount(false)
{
	//if (bout != NULL) bout << "Creating lock " << this << ": " << m_name << endl;
	if (m_createStack && LockDebugStackCrawls()) m_createStack->Update(2);
}

DebugLockNode::~DebugLockNode()
{
	//if (bout != NULL) bout << "Destroying lock " << this << ": " << m_name << endl;
	if (LockGraph()) {
		// Remove associations with all other locks.
		const int32_t NT = m_links->targets.CountItems();
		const int32_t NS = m_links->sources.CountItems();
		int32_t i;
		for (i=0; i<NT; i++) {
			DebugLockNode* target = m_links->targets.KeyAt(i);
			if (target && target->m_links) {
				DebugLockNode* this_node = this;
				target->m_links->sources.RemoveItemFor(this_node);
			}
		}
		for (i=0; i<NS; i++) {
			DebugLockNode* source = m_links->sources.ItemAt(i);
			if (source && source->m_links) {
				DebugLockNode* this_node = this;
				source->m_links->targets.RemoveItemFor(this_node);			
			}
		}
		RemLockHeld(this);
		UnlockGraph();
	}
	
	delete m_links;
	delete m_createStack;
}

void DebugLockNode::Delete()
{
	DecRefs();
	
	//if (bout != NULL) bout << "Deleting lock " << this << " (taken=" << taken << "): " << m_name << endl;

	PrintContentionIfNeeded();
}

int32_t DebugLockNode::IncRefs() const
{
	return atomic_add(&m_refs, 1);
}

int32_t DebugLockNode::DecRefs() const
{
	const int32_t c = atomic_add(&m_refs, -1);
	//if (bout != NULL) bout << "Decrementing lock " << this << "(count was " << c << "): " << m_name << endl;
	if (c == 1) delete const_cast<DebugLockNode*>(this);
	return c;
}

const char* DebugLockNode::Type() const
{
	return m_type.String();
}

const void* DebugLockNode::Addr() const
{
	return m_addr;
}

const char* DebugLockNode::Name() const
{
	return m_name.String();
}

uint32_t DebugLockNode::GlobalFlags() const
{
	return m_globalFlags;
}

const SCallStack* DebugLockNode::CreationStack() const
{
	return m_createStack;
}

nsecs_t DebugLockNode::CreationTime() const
{
	return m_createTime;
}

inline bool DebugLockNode::LockGraph() const
{
	if (!m_links) return false;
	LockGlobalGraph();
	return true;
}

inline void DebugLockNode::UnlockGraph() const
{
	UnlockGlobalGraph();
}

bool DebugLockNode::AddToGraph()
{
	bool succeeded = true;
	
	// Add this block to the block graph, checking to see if it creates
	// a cycle.
	SSortedVector<DebugLockNode*>* held = LocksHeld();
	if (held) {
		const int32_t N = held->CountItems();
		for (int32_t i=0; i<N; i++) {
			DebugLockNode* source = held->ItemAt(i);
			if (source && source->m_links) {
			
				DebugLockNode* this_node = this;
				const int32_t idx = source->m_links->targets.IndexOf(this_node);
				SVector<const link_info*>* deadlock = NULL;
				if (idx < 0) {
					// This dependency does not currently exist.  We need
					// to add it, but before doing so make sure it won't create
					// a deadlock -- we never want to have a cyclic graph.
					deadlock = FindSourceLock(this_node, source);
					if (!deadlock) {
						link_info info(this_node, source);
						source->m_links->targets.AddItem(this_node, info);
					} else {
						// NOTE THAT THERE ARE MANY POTENTIAL DEADLOCKS HERE!
						// We really shouldn't do anything that might acquire another
						// lock while holding the graph lock...  however, we're just
						// doing this to -report- a friggin' deadlock, so who cares??
						BStringIO* dirtyADSHack = new BStringIO();
						sptr<ITextOutput> msg = dirtyADSHack;
						msg << "Thread " << SysCurrentThread()
							<< " created a potential deadlock!" << endl << endl
							<< "While holding " << *source << endl;
						msg->MoveIndent(1);
						source->PrintStacksToStream(msg);
						msg->MoveIndent(-1);
						msg	<< "attempting to acquire " << *this << endl << endl
							<< "Previously these were acquired as:" << endl;
						msg->MoveIndent(1);
						const int32_t N = deadlock->CountItems();
						for (int32_t i=0; i<N; i++) {
							const link_info* info = deadlock->ItemAt(i);
							msg << "Holding " << (*info->source) << endl
								<< "acquired " << (*info->target) << endl;
							msg->MoveIndent(1);
							msg << "Occurred " << info->count
								<< " times; first seen in thread "
								<< info->thread << ":" << endl;
							if (LockDebugStackCrawls()) {
								info->stack.Print(msg);	msg << endl;
								info->stack.LongPrint(msg);
							} else {
								msg << "(Stack crawls not enabled.)" << endl;
							}
							msg->MoveIndent(-1);
						}
						msg->MoveIndent(-1);
						UnlockGlobalGraph();
						printf("%s", dirtyADSHack->String());
						delete deadlock;
						ErrFatalError(dirtyADSHack->String());
						LockGlobalGraph();
						succeeded = false;
					}
				} else {
					source->m_links->targets.EditValueAt(idx).count++;
				}
				if (deadlock == NULL && m_links->sources.IndexOf(source) < 0) {
					m_links->sources.AddItem(source);
				}
			}
		}
	}
	
	return succeeded;
}

void DebugLockNode::RegisterAsHeld()
{
	AddLockHeld(this);
}

void DebugLockNode::UnregisterAsHeld()
{
	RemLockHeld(this);
}

void DebugLockNode::SetMaxContention(int32_t c)
{
	if (c > 0) {
		// This thread will block.
		bool changed = false;

		if (LockContention()) {
			m_blockCount += 1;
			gBlockCount += 1;
			if (m_inBlockCount) {
				UnregisterBlockCount(this);
				m_inBlockCount = false;
				DecRefs();
			}
			if (RegisterBlockCount(this)) {
				changed = true;
				m_inBlockCount = true;
				IncRefs();
			}
			UnlockContention();
		}

		if (changed) PrintContentionIfNeeded();
	}

	if (c <= m_maxContention) return;

	bool changed = false;

	//printf("Lock %p contention increased to %ld\n", this, c);

	if (LockContention()) {
		if (c > m_maxContention) {
			if (m_inContention) {
				UnregisterContention(this);
				m_inContention = false;
				DecRefs();
			}
			m_maxContention = c;
			if (RegisterContention(this)) {
				changed = true;
				m_inContention = true;
				IncRefs();
			}
		}
		UnlockContention();
	}

	if (changed) PrintContentionIfNeeded();
}

int32_t DebugLockNode::MaxContention() const
{
	return m_maxContention;
}

int32_t DebugLockNode::BlockCount() const
{
	return m_blockCount;
}

void DebugLockNode::RemoveFromContention()
{
	ErrFatalErrorIf(!m_inContention, "Debug node not in contention list!");
	m_inContention = false;
	DecRefs();
}

void DebugLockNode::RemoveFromBlockCount()
{
	ErrFatalErrorIf(!m_inBlockCount, "Debug node not in block count list!");
	m_inBlockCount = false;
	DecRefs();
}

void DebugLockNode::PrintToStream(const sptr<ITextOutput>& io) const
{
	io << m_type << "(" << m_addr << " \"" << m_name << "\"";
	PrintSubclassToStream(io);
	io << ")";
}

void DebugLockNode::PrintContentionToStream(const sptr<ITextOutput>& io)
{
	bool found = false;
	if (LockContention()) {
		const nsecs_t curTime = SysGetRunTime();

		io << endl << "Lock contention summary in process " << SysProcessName() << " (#" << SysProcessID() << "):" << endl << endl << indent;

		if (gHighContention) {
			const int32_t N = gHighContention->CountItems();
			for (int32_t i=0; i<N; i++) {
				DebugLockNode* node = gHighContention->ItemAt(i);
				if (node) {
					if (!found) {
						io << "Top " << N << " locks with highest contention:" << endl;
						found = true;
					}
					io->MoveIndent(1);
					io << *node << " max contention is " << node->MaxContention() << endl;
					if (node->CreationStack()) {
						io << "Created at:" << endl;
						io->MoveIndent(1);
						if (LockDebugStackCrawls()) {
							node->CreationStack()->Print(io);	io << endl;
							node->CreationStack()->LongPrint(io);
						} else {
							io << "(Stack crawls not enabled.)" << endl;
						}
						io->MoveIndent(-1);
					}
					io->MoveIndent(-1);
				}
			}
		}
		if (gHighBlockCount) {
			if (found) io << endl;
			found = false;
			const int32_t N = gHighBlockCount->CountItems();
			for (int32_t i=0; i<N; i++) {
				DebugLockNode* node = gHighBlockCount->ItemAt(i);
				if (node) {
					if (!found) {
						io << "Top " << N << " locks causing threads to block:" << endl;
						found = true;
					}
					io->MoveIndent(1);
					io << *node << " blocked " << node->BlockCount() << " times";
					const nsecs_t duration = (curTime-node->CreationTime()) / B_S2NS(1);
					if (duration > 0) io << " (" << (node->BlockCount()/duration) << " block/sec)";
					io << endl;
					if (node->CreationStack()) {
						io << "Created at:" << endl;
						io->MoveIndent(1);
						if (LockDebugStackCrawls()) {
							node->CreationStack()->Print(io);	io << endl;
							node->CreationStack()->LongPrint(io);
						} else {
							io << "(Stack crawls not enabled.)" << endl;
						}
						io->MoveIndent(-1);
					}
					io->MoveIndent(-1);
				}
			}
		}
		if (found) io << endl;
		const nsecs_t duration = (curTime-gStartTime) / B_S2NS(1);
		if (duration > 0) {
			io << "Number of threads blocked per second: " << (gBlockCount/duration) << endl;
		}
		io << dedent;
	}
	UnlockContention();
}

void DebugLockNode::PrintStacksToStream(sptr<ITextOutput>& io) const
{
	if (CreationStack()) {
		io << "Created at:" << endl;
		io->MoveIndent(1);
		if (LockDebugStackCrawls()) {
			CreationStack()->Print(io);	io << endl;
			CreationStack()->LongPrint(io);
		} else {
			io << "(Stack crawls not enabled.)" << endl;
		}
		io->MoveIndent(-1);
	}
}

void DebugLockNode::PrintSubclassToStream(const sptr<ITextOutput>&) const
{
}

DebugLock::DebugLock(	const char* type, const void* addr,
						const char* name, uint32_t debug_flags)
	:	DebugLockNode(type, addr, name, debug_flags),
		m_held(0), m_owner(SysHandle(B_ERROR)),
		m_ownerStack(CreationStack() ? new(B_SNS(std::) nothrow) SCallStack : NULL),
		m_contention(0), m_deleted(false)
{
	dbg_init_gehnaphore(&m_gehnaphore);
	//SysSempahoreCreateEZ(1, &m_sem);
}

DebugLock::~DebugLock()
{
	// if (!m_deleted) SysSemaphoreDestroy(m_sem);
	delete m_ownerStack;
}

void DebugLock::Delete()
{
	// When the lock is held, deleting it is an error if:
	// * The LOCK_ANYONE_CAN_DELETE flag is not set; and
	//   * This thread is not the current owner of the lock; or
	//   * The LOCK_CAN_DELETE_WHILE_HELD flag is not set.
	if (m_owner != SysHandle(B_ERROR)) {
		if ((GlobalFlags()&LOCK_ANYONE_CAN_DELETE) == 0
				&& ((GlobalFlags()&LOCK_CAN_DELETE_WHILE_HELD) == 0
					|| m_owner != SysCurrentThread())) {
			BStringIO* dirtyADSHack = new BStringIO();
			sptr<ITextOutput> msg = dirtyADSHack;
			msg << "Thread " << SysCurrentThread() << " deleting " << *this << endl
				<< "Currently held by thread " << m_owner << endl;
			PrintStacksToStream(msg);
			printf("%s", dirtyADSHack->String());
			ErrFatalError(dirtyADSHack->String());
		}
		if (m_owner == SysCurrentThread()) {
			if (LockGraph()) {
				UnregisterAsHeld();
				UnlockGraph();
			}
		} else {
			ErrFatalError("DebugLock: need to implement LOCK_ANYONE_CAN_DELETE!");
		}
	}
	
	m_deleted = true;
	//SysSemaphoreDestroy(m_sem);
	
	DebugLockNode::Delete();
}

status_t DebugLock::Lock(uint32_t flags, nsecs_t timeout, uint32_t debug_flags)
{
	return do_lock(flags, timeout, debug_flags);
}

status_t DebugLock::do_lock(uint32_t flags, nsecs_t timeout, uint32_t debug_flags, bool restoreOnly)
{
	if (m_deleted) {
		BStringIO* dirtyADSHack = new BStringIO();
		sptr<ITextOutput> msg = dirtyADSHack;
		msg << "Thread " << SysCurrentThread() << " attempting to acquire deleted " << *this << endl;
		PrintStacksToStream(msg);
		printf("%s", dirtyADSHack->String());
		ErrFatalError(dirtyADSHack->String());
	} else if (m_owner == SysCurrentThread()) {
		BStringIO* dirtyADSHack = new BStringIO();
		sptr<ITextOutput> msg = dirtyADSHack;
		msg << "Thread " << m_owner << " attempted to acquire " << *this
			<< " multiple times" << endl;
		PrintStacksToStream(msg);
		printf("%s", dirtyADSHack->String());
		ErrFatalError(dirtyADSHack->String());
	}
	
	if (timeout != 0
			&& ((GlobalFlags()|debug_flags)&LOCK_SKIP_DEADLOCK_CHECK) == 0
			&& LockGraph()) {
		AddToGraph();
		UnlockGraph();
	}
	
	SetMaxContention(atomic_add(&m_contention, 1));
	
	status_t err = B_OK;
	if (!restoreOnly) {
		dbg_lock_gehnaphore(&m_gehnaphore);
		// SysSemaphoreWait(m_sem, B_WAIT_FOREVER, 0);
		ErrFatalErrorIf(atomic_or(&m_held, 1) != 0, "Lock gehnaphore returned while lock still held!");
	}

	if (err == B_OK) {
		if (((GlobalFlags()|debug_flags)&LOCK_DO_NOT_REGISTER_HELD) == 0
				&& LockGraph()) {
			RegisterAsHeld();
			UnlockGraph();
		}
		m_owner = SysCurrentThread();
		m_ownerFlags = debug_flags;
		if (m_ownerStack && LockDebugStackCrawls()) m_ownerStack->Update(2);
	} else if (err != B_TIMED_OUT && (timeout != 0 || err != B_WOULD_BLOCK)) {
		BStringIO* dirtyADSHack = new BStringIO();
		sptr<ITextOutput> msg = dirtyADSHack;
		if (err != B_NO_INIT) msg << "Error \"";
		else msg << "Warning \"";
		msg << strerror(err) << "\" attempting to acquire " << *this << endl;
		PrintStacksToStream(msg);
		printf("%s", dirtyADSHack->String());
		ErrFatalErrorIf(err != B_NO_INIT, dirtyADSHack->String());
	}
	
	return err;
}

status_t DebugLock::Unlock()
{
	return do_unlock();
}

status_t DebugLock::do_unlock(bool removeOnly)
{
	if (m_deleted) {
		BStringIO* dirtyADSHack = new BStringIO();
		sptr<ITextOutput> msg = dirtyADSHack;
		msg << "Thread " << SysCurrentThread() << " attempting to release deleted " << *this << endl;
		PrintStacksToStream(msg);
		printf("%s", dirtyADSHack->String());
		ErrFatalError(dirtyADSHack->String());
	} else if (((int32_t)m_owner) == B_ERROR) {
		BStringIO* dirtyADSHack = new BStringIO();
		sptr<ITextOutput> msg = dirtyADSHack;
		msg << "Thread " << SysCurrentThread() << " releasing "
			<< *this << " that is not held" << endl;
		PrintStacksToStream(msg);
		printf("%s", dirtyADSHack->String());
		ErrFatalError(dirtyADSHack->String());
	} else if (m_owner != SysCurrentThread()) {
		if (((GlobalFlags()|m_ownerFlags)&LOCK_ANYONE_CAN_UNLOCK) == 0) {
			BStringIO* dirtyADSHack = new BStringIO();
			sptr<ITextOutput> msg = dirtyADSHack;
			msg << "Thread " << SysCurrentThread() << " releasing " << *this << endl
				<< "Held by thread " << m_owner << endl;
			PrintStacksToStream(msg);
			printf("%s", dirtyADSHack->String());
			ErrFatalError(dirtyADSHack->String());
		}
	}
	
	m_owner = SysHandle(B_ERROR);
	
	if (LockGraph()) {
		UnregisterAsHeld();
		UnlockGraph();
	}
	
	m_held = 0;

	atomic_add(&m_contention, -1);
	
	status_t err = B_OK;
	if (!removeOnly) {
		dbg_unlock_gehnaphore(&m_gehnaphore);
		// SysSemaphoreSignal(m_sem);
	}
	
	if (err != B_OK) {
		BStringIO* dirtyADSHack = new BStringIO();
		sptr<ITextOutput> msg = dirtyADSHack;
		msg << "Error \"" << strerror(err) << "\" attempting to release " << *this;
		PrintStacksToStream(msg);
		ErrFatalError(dirtyADSHack->String());
	}
	
	return err;
}

bool DebugLock::IsLocked() const
{
	return (m_owner != SysHandle(B_ERROR));
}

const SCallStack* DebugLock::OwnerStack() const
{
	return m_ownerStack;
}

void DebugLock::RestoreOwnership()
{
	do_lock(B_TIMEOUT, B_INFINITE_TIMEOUT, 0, true);
}

volatile int32_t* DebugLock::RemoveOwnership()
{
	do_unlock(true);
	return &m_gehnaphore;
}

void DebugLock::PrintStacksToStream(sptr<ITextOutput>& io) const
{
	DebugLockNode::PrintStacksToStream(io);
	if (OwnerStack() && ((int32_t)m_owner) != B_ERROR) {
		io << "Locked by thread " << m_owner << " at:" << endl;
		io->MoveIndent(1);
		OwnerStack()->Print(io); io << endl;
		OwnerStack()->LongPrint(io);
		io->MoveIndent(-1);
	}
}

void DebugLock::PrintSubclassToStream(const sptr<ITextOutput>& io) const
{
	// io << ", sem=" << m_sem;
}

#if _SUPPORTS_NAMESPACE
} }	// namespace palmos::support
#endif

#endif	// SUPPORTS_LOCK_DEBUG
