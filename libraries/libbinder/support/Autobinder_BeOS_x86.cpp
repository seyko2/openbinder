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

static inline status_t call_effect_func(const effect_action_def& action,
		const effect_call_state& state, const sptr<IInterface>& target, SValue* out)
{
	status_t err = B_OK;
	SValue returnValue;
	unsigned char * object = NULL;
	if (action.parameters) {
		object = reinterpret_cast<unsigned char *>(target.ptr());
		object += action.parameters->classOffset;
	}
	
	switch (state.which)
	{
		case B_INVOKE_ACTION:
			if (action.invoke) {
				returnValue = action.invoke(target, state.args);
			}
			else if (action.parameters && action.parameters->invoke) {
				err = InvokeAutobinderFunction(	action.parameters->invoke,
												object,
												state.args,
												&returnValue);
			}
			else {
				return B_MISMATCHED_VALUES;
			}
			out->JoinItem(state.outKey, returnValue);
			break;
		
		case B_PUT_ACTION:
			if (action.put) {
				err = action.put(target, state.args);
			}
			else if (action.parameters && action.parameters->put) {
				err = InvokeAutobinderFunction(	action.parameters->put,
												object,
												SValue(SValue::Int32(0), state.args),
												&returnValue);
			}
			else {
				return B_MISMATCHED_VALUES;
			}
			break;
		
		case B_GET_ACTION:
			if (action.get) {
				returnValue = action.get(target);
			}
			else if (action.parameters && action.parameters->get) {
				err = InvokeAutobinderFunction(	action.parameters->get,
												object,
												SValue::undefined,
												&returnValue);
			}
			else {
				return B_MISMATCHED_VALUES;
			}
			out->JoinItem(state.outKey, returnValue);
			break;
		default:
			return B_BAD_VALUE;
	}
	
//	SValue nullll("NULL");
//	bout << "out is " << (out ? *out : nullll) << endl;
	
	return err;
}

static uint32_t finish_logic(bool hasIn, bool hasOut, const effect_action_def &action)
{
	bool hasPut, hasGet, hasInvoke;
	if (action.parameters) {
		hasPut = action.parameters->put != NULL;
		hasGet = action.parameters->get != NULL;
		hasInvoke = action.parameters->invoke != NULL;
	} else {
		hasPut = action.put != NULL;
		hasGet = action.get != NULL;
		hasInvoke = action.invoke != NULL;
	}
					
	// Select the appropriate action to be performed.  Only one 
	// hook will be called, whichever best matches the effect arguments. 

	// Do an invoke if it is defined and we have both an input 
	// and output, or only an input and no "put" action. 
	if (hasInvoke && hasIn && (hasOut || !hasPut)) 
		return B_INVOKE_ACTION;

	// Do a put if it is defined and we have an input. 
	else if (hasPut && hasIn) 
		return B_PUT_ACTION;
	
	// Do a get if it is defined and we have an output. 
	else if (hasGet && hasOut) 
		return B_GET_ACTION;
		
	else return B_NO_ACTION;
}

status_t execute_effect(const sptr<IInterface>& target,
						const SValue &in, const SValue &inBindings,
						const SValue &outBindings, SValue *out,
						const effect_action_def* actions, size_t num_actions,
						uint32_t flags)
{
	const SValue told(inBindings * in);
	const size_t inN = told.CountItems();
	const size_t outN = outBindings.CountItems();
	effect_call_state state;
	bool hasIn, hasOut;
	status_t result = B_OK;
	
	// Optimize for the common case of the actions being in value-sorted order,
	// and only one action is being performed.
	if (	(flags&B_ACTIONS_SORTED_BY_KEY) != 0 &&
			(hasIn=(inN <= 1)) &&
			(hasOut=(outN <= 1)) ) {
		SValue key;
		void* cookie;
		if (inN) {
			cookie = NULL;
			told.GetNextItem(&cookie, &key, &state.args);
		}
		if (outN) {
			SValue tmpKey;
			cookie = NULL;
			outBindings.GetNextItem(&cookie, &state.outKey, &tmpKey);
			// If this is a get action, store the key; if it could be an
			// invoke, make sure the output key matches the input.
			if (!inN) key = tmpKey;
			else if (key != tmpKey) key.Undefine();
		}
		
		#if 0
		bout	<< "execute_effect() short path:" << endl << indent
				<< "Told: " << told << endl
				<< "outBindings: " << outBindings << endl
				<< "key: " << key << endl
				<< "args: " << state.args << endl
				<< "outKey: " << state.outKey << endl << dedent;
		#endif
		
		if (key.IsDefined()) {
			ssize_t mid, low = 0, high = ((ssize_t)num_actions)-1;
			while (low <= high) {
				mid = (low + high)/2;
				const int32_t cmp = actions[mid].key().Compare(key);
				if (cmp > 0) {
					high = mid-1;
				} else if (cmp < 0) {
					low = mid+1;
				} else {
					// Found the action; determine which function to call.
					if (!actions[mid].put && !actions[mid].invoke
							 && !(actions[mid].parameters && actions[mid].parameters->put)
							 && !(actions[mid].parameters && actions[mid].parameters->invoke))
						hasIn = false;
					if (!actions[mid].get && !actions[mid].invoke
							 && !(actions[mid].parameters && actions[mid].parameters->get)
							 && !(actions[mid].parameters && actions[mid].parameters->invoke))
						hasOut = false;
					#if 0
					bout	<< "Found action #" << mid << ": in=" << hasIn
							<< ", out=" << hasOut << endl;
					#endif
					
					state.which = finish_logic(hasIn && inN>0, hasOut && outN>0, actions[mid]);
					return call_effect_func(actions[mid], state, target, out);
				}
			}
		}
		
		#if 0
		bout	<< "execute_effect() short path FAILED:" << endl << indent
				<< "Told: " << told << endl
				<< "outBindings: " << outBindings << endl
				<< "key: " << key << endl
				<< "args: " << state.args << endl
				<< "outKey: " << state.outKey << endl << dedent;
		#endif
	}
	
	// Unoptimized version when effect_action_def array is not
	// in sorted order or multiple bindings are supplied.
	
	// Iterate through all actions and execute in order.

	while (num_actions > 0 && result == B_OK) {

	#if 0
	bout	<< "execute_effect() long path:" << endl << indent
			<< "action.key: " << actions->key() << endl
			<< "Told: " << told << endl
			<< "outBindings: " << outBindings << endl
			<< "args: " << state.args << endl
			<< "outKey: " << state.outKey << endl << dedent;
	#endif
	
		// First check to see if there are any inputs or outputs in
		// the effect for this action binding.
		hasIn		= (	(actions->put || actions->invoke
								|| (actions->parameters && actions->parameters->put)
								|| (actions->parameters && actions->parameters->invoke))
							&& (state.args = told[actions->key()]).IsDefined() );
		hasOut	= (actions->get || actions->invoke
								|| (actions->parameters && actions->parameters->get)
								|| (actions->parameters && actions->parameters->invoke));
		
		if (hasOut) {
			state.outKey = (outBindings * SValue(actions->key(), SValue::wild)); // ??? outBindings[actions->key()]; 
			if (state.outKey.IsDefined()) {
				void* cookie = NULL;
				SValue key, val;
				state.outKey.GetNextItem(&cookie, &key, &val);
				state.outKey = key;
			} else {
				hasOut = false;
			}
		}
		
		state.which = finish_logic(hasIn, hasOut, *actions);
		if (state.which != B_NO_ACTION) {
			result = call_effect_func(*actions, state, target, out);
		}
		
		actions = reinterpret_cast<const effect_action_def*>(
			reinterpret_cast<const uint8_t*>(actions) + actions->struct_size);
		num_actions--;
	}
	
	return result;
}


#if _SUPPORTS_NAMESPACE
namespace palmos {
namespace osp {
#endif


#if AUTOBINDER_INCLUDE_DUMMYCLASS
// ======================================================================
// dummi class
// ======================================================================
void BAutobinderDummiClass::Method()
{
	#if DEBUG
		ErrFatalError("Why are you calling this function?");
	#endif
}
#endif //AUTOBINDER_INCLUDE_EVERYTHING

#if _SUPPORTS_NAMESPACE
} } //End palmos::osp
#endif


#if AUTOBINDER_INCLUDE_CLEANUPAFTERCALL
void
cleanup_after_call(const BEffectMethodDef *def, uint32_t stackAmt, unsigned char *place)
{
	uint32_t i;
	unsigned char * q = place + stackAmt;
	
	switch (def->returnType)
	{
		case B_STRING_TYPE:
			q -= sizeof(SString);
			((SString *)q)->~SString();
			break;
		default:
			break;
	}

	for (i=0; i<def->paramCount; i++) {
		switch (def->paramTypes[i].type)
		{
			case B_INT32_TYPE:
				break;
			case B_STRING_TYPE:
				q -= sizeof(SString);
				((SString *)q)->~SString();
				break;
			case B_VALUE_TYPE:
				break;
			default:
				return ;
		}
	}
}
#endif //AUTOBINDER_INCLUDE_CLEANUPAFTERCALL

#if AUTOBINDER_INCLUDE_FILLINPARAMS
status_t
fill_in_params(const BEffectMethodDef *def, uint32_t stackAmt, const SValue *args, unsigned char *place, uint32_t object)
{
	uint32_t i;
	int32_t data_int32;
	
	// Bases for where temporary objects go
	unsigned char * p = place;
	unsigned char * q = place + stackAmt;
	
	// Make a place for the return value if it's an object
	switch (def->returnType)
	{
		case B_STRING_TYPE:
			q -= sizeof(SString);
			*((int*)q) = 0x11111111;
			*((SString**)p) = (SString *)q;
			p += sizeof(SString*);
			break;
		default:
			break;
	}
	
	// Add the 'this' pointer
	*((uint32_t*)p) = object;
	p += sizeof(uint32_t);
	
	// Create temporary objects
	for (i=0; i<def->paramCount; i++) {
		
		SValue val((*args)[i]);
		if (!val.IsDefined()) {
			return B_NAME_NOT_FOUND;
		}
		
		switch (def->paramTypes[i].type)
		{
			case B_INT32_TYPE:
				data_int32 = val.AsInt32();
				*((int32_t*)p) = data_int32;
				p += sizeof(int32_t);
				break;
			case B_STRING_TYPE:
				q -= sizeof(SString);
				new(q) SString(val.AsString());
				*((SString**)p) = (SString *)q;
				p += sizeof(SString *);
				break;
			case B_INT8_TYPE:
				data_int32 = val.AsInt32() & 0x000000ff;
				*((int32_t*)p) = data_int32;
				p += sizeof(int32_t);
				break;
			case B_INT16_TYPE:
				data_int32 = val.AsInt32() & 0x0000ffff;
				*((int32_t*)p) = data_int32;
				p += sizeof(int32_t);
				break;
			default:
				return B_BAD_TYPE;
		}
	}
	
	return B_OK;
}
#endif //AUTOBINDER_INCLUDE_FILLINPARAMS

#if AUTOBINDER_INCLUDE_MAKERETURNVALUE
void
make_return_value(unsigned int eax, unsigned char * q, uint32_t returnType, SValue * rv)
{
	switch (returnType)
	{
		case B_INT32_TYPE:
			*rv = SValue::Int32(eax);
			break;
		case B_STRING_TYPE:
			q -= sizeof(SString);
			// printf("making return SValue SString (0x%08x): \"%s\"\n", (int) q, ((SString *)q)->String());
			*rv = SValue::String(*((SString *)q));
			break;
		default:
			break;
	}
	
	// bout << "make_return_value returning: " << *rv << endl;
}
#endif //AUTOBINDER_INCLUDE_MAKERETURNVALUE

#if AUTOBINDER_IMPLEMENT_INVOKEAUTOBINDERFUNCTION
// The public function:
status_t
InvokeAutobinderFunction(	const BEffectMethodDef	* def,
							void					* object,
							const SValue			& args,
							SValue					* returnValue)
{
	// bout << "Autobinder invoking args: " << args << endl;
	
	status_t err;
	
	// Find the function to call, and maybe offset the object pointer
	int32_t func;
	unsigned char * obj = (unsigned char *)object;
	// If the function isn't virtual, use this
	if (def->pmf.index < 0) {
		func = def->pmf.func;
	}
	else {
		int32_t **vtable_ptr = (int32_t **)(obj+def->pmf.offset);
		BPointerToMemberFunction *vtable = (BPointerToMemberFunction *) *vtable_ptr;
		// Potential source of problems:
		// I'm adding this because it makes it work, but I don't understand why I need to subtract one.
		// BPointerToMemberFunction &vtable_entry = vtable[def->pmf.index];
		BPointerToMemberFunction &vtable_entry = vtable[def->pmf.index-1];
		func = vtable_entry.func;
		obj += vtable_entry.delta;
	}
	
	// printf("going to call function 0x%08x\n", (int) func);
	
	// printf("\tobj: 0x%08x\n", (int)obj);
	
	// Figure out how much stack space we need
	unsigned char * place;
	uint32_t i;
	uint32_t stackAmt = 0;
	uint32_t isReturnPointer = 0;
	
	switch (def->returnType)
	{
		case B_STRING_TYPE:
			stackAmt += sizeof(SString);
			stackAmt += sizeof(SString *);
			isReturnPointer = 1;
			break;
		default:
			break;
	}
	
	stackAmt += sizeof(uint32_t); // space for 'this' pointer
	
	for (i=0; i<def->paramCount; i++) {
		SValue val(args[i]);
		if (!val.IsDefined()) {
			// bout << "missing" << dedent << endl;
			return B_NAME_NOT_FOUND;
		}

		switch (def->paramTypes[i].type)
		{
			case B_INT8_TYPE:
			case B_INT16_TYPE:
			case B_INT32_TYPE:
				stackAmt += sizeof(int32_t);
				break;
			case B_STRING_TYPE:
				stackAmt += sizeof(SString);
				stackAmt += sizeof(SString *);
				break;
			case B_VALUE_TYPE:
				break;
			default:
				// bout << "unknown type" << dedent << endl;
				return B_BAD_TYPE;
		}
	}

#if defined(__INTEL__) && defined(__GNUC__) && !defined(_NO_INLINE_ASM)
	__asm__ __volatile__ (
		// Make room on the stack
		"movl %%esp, %%edx;"

		"subl %1, %%esp;"
		"movl %%esp, %2;"
		
		// err = fill_in_params(BEffectMethodDef *def, uint32_t stackAmt, const SValue *args, unsigned char *place, uint32_t object)
		"movl %%esp, %%edx;"

		"pushl %5;"
		"pushl %2;"
		"pushl %4;"
		"pushl %1;"
		"pushl %3;"
 		"call fill_in_params__Q36palmos3osp10AutobinderPCQ36palmos7support16BEffectMethodDefUlPCQ36palmos7support6BValuePUcUl;"
		"addl $0x14, %%esp;"
		"movl %%eax, %0;"

		"movl %%esp, %%edx;"
		 :"=&r"(err)			// 0
		 :"m"(stackAmt),		// 1
		  "m"(place),			// 2
		  "m"(def),				// 3
		  "m"(&args),			// 4
		  "m"(obj)				// 5
		 :"%edx"
	);
		
	if (err != B_OK) return err;
		
	__asm__ __volatile__ (
		// Call the function
		"movl %%esp, %%edx;"
		"call %4;"
		
		// Make the return SValue
		// make_return_value(unsigned int eax, unsigned char * q, uint32_t returnType, SValue * rv)
		"movl %%eax, %%ecx;"
		"pushl %8;"
		"pushl %6;"
		"movl %1, %%eax;"
		"movl %3, %%edx;"
		"addl %%edx, %%eax;"
		"pushl %%eax;"
		"pushl %%ecx;"
		"call make_return_value__Q36palmos3osp10AutobinderUiPUcUlPQ36palmos7support6SValue;"
		"addl $0x10, %%esp;"
		
		// Clean up the temporary variables we created.
		// cleanup_after_call(BEffectMethodDef *def, uint32_t stackAmt, unsigned char *place)
		"pushl %1;"
		"pushl %3;"
		"pushl %2;"
		"call cleanup_after_call__Q36palmos3osp10AutobinderPCQ36palmos7support16BEffectMethodDefUlPUc;"
		"addl $0x0c, %%esp;"
		
		// Done
		"ERROR:"
		"movl %3, %%eax;"
		"movl %7, %%edx;"
		"cmpl $0x00, %%edx;"
		"je POP_STACK_WHEN_DONE;"
		"subl $0x04, %%eax;"
		"POP_STACK_WHEN_DONE:"
		"addl %%eax, %%esp;"
		
		 :"=r"(err)						// 0
		 :"m"(place),					// 1
		  "m"(def),						// 2
		  "m"(stackAmt),				// 3
		  "m"(func),					// 4
		  "m"(obj),						// 5
		  "m"(def->returnType),			// 4
		  "m"(isReturnPointer),			// 7
		  "m"(returnValue)				// 8
		 :"%eax", "%ecx", "%edx"
	);
#endif /* __INTEL__ */

	// bout << "returnValue: " << *returnValue << endl;
	
	return B_OK;
}
#else //AUTOBINDER_IMPLEMENT_INVOKEAUTOBINDERFUNCTION

status_t
InvokeAutobinderFunction(	const BEffectMethodDef	* def,
							void					* object,
							const SValue			& args,
							SValue					* returnValue)
{
	ErrFatalError("InvokeAutobinderFunction not implemented!");
	return 0;
}
#endif //AUTOBINDER_IMPLEMENT_INVOKEAUTOBINDERFUNCTION

#if AUTOBINDER_INCLUDE_PERFORMREMOTEBINDERCALL
void
PerformRemoteBinderCall(	int						ebp,
							const BEffectMethodDef	*def,
							unsigned char			*returnValue,
							const sptr<IBinder>&		remote,
							SValue					binding,
							uint32_t					which
							)
{
	SValue returned;
	// Let's just get this out of the way
	if (which == B_GET_ACTION) {
		returned = remote->Get(binding);
	}
	else {	
		int rvskip;
		switch (def->returnType)
		{
			case B_VALUE_TYPE:
				rvskip = 4;
				break;
			default:
				rvskip = 0;
		}
		
		// ptr now equals the first parameter
		// +4 is for the this pointer i think
		//unsigned char *ptr = (unsigned char *)(ebp+8+rvskip);
		unsigned char *ptr = (unsigned char *)(ebp+8+4+rvskip);
		
		
		// Go through the info array, pulling out values and adding them to the argv SValue
		// (and printing them too)
		SValue argv;
		const SValue ** bvalue;
		uint32_t i;
		unsigned char * p = ptr;
		for (i=0; i<def->paramCount; i++) {
			switch (def->paramTypes[i].type)
			{
				case B_INT32_TYPE:
					argv.JoinItem(i, SValue::Int32(*(int*)p));
					p += sizeof(int32_t);
					break;
				case B_STRING_TYPE:
					argv.JoinItem(i, SValue::String((const char *) *(int*)p));
					p += sizeof(char *);
					break;
				case B_VALUE_TYPE:
					bvalue = (const SValue **)p;
					argv.JoinItem(i, **bvalue);
					p += sizeof(SValue *);
					break;
				default:
					return;
			}
		}
		
		// Call the remote function
		switch (which)
		{
			case B_INVOKE_ACTION:
				// bout << "remote->Invoke: binding: " << binding << " argv: " << argv << endl;
				returned = remote->Invoke(binding, argv);
				break;
			case B_PUT_ACTION:
				// bout << "remote->Put: " << SValue(binding, argv[SValue::Int32(0)]) << endl;
				remote->Put(SValue(binding, argv[SValue::Int32(0)]));
				break;
			default:
				// bout << "invalid which (" << which << ")" << endl;
				return;
		}
	}

	// bout << "returned: " << returned << endl;

	if (returnValue) {
		// Do the return value
		switch (def->returnType)
		{
			case B_INT32_TYPE:
				*((uint32_t *) returnValue) = returned.AsInt32();
				break;
			case B_STRING_TYPE:
				((SString *) returnValue)->SetTo(returned.AsString());
				break;
			case B_VALUE_TYPE:
				*((SValue *) returnValue) = returned;
				break;
			default:
				// printf("unknown return type\n");
				return;
		}
	}
}
#endif //AUTOBINDER_INCLUDE_PERFORMREMOTEBINDERCALL


