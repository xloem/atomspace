/*
 * opencog/atoms/base/ClassServer.cc
 *
 * Copyright (C) 2002-2007 Novamente LLC
 * Copyright (C) 2008 by OpenCog Foundation
 * Copyright (C) 2009,2017 Linas Vepstas <linasvepstas@gmail.com>
 * All Rights Reserved
 *
 * Written by Thiago Maia <thiago@vettatech.com>
 *            Andre Senna <senna@vettalabs.com>
 *            Gustavo Gama <gama@vettalabs.com>
 *            Linas Vepstas <linasvepstas@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License v3 as
 * published by the Free Software Foundation and including the exceptions
 * at http://opencog.org/wiki/Licenses
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program; if not, write to:
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "ClassServer.h"

#include <exception>

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/atom_types/atom_types.h>
#include <opencog/atoms/base/Atom.h>
#include <opencog/atoms/value/Value.h>

//#define DPRINTF printf
#define DPRINTF(...)

using namespace opencog;

ClassServer::ClassServer(const NameServer & nameServer):
	_nameServer(nameServer)
{}

template<typename T>
void ClassServer::splice(std::vector<T>& methods, Type t, T fact)
{
	// N.B. it is too late to synchronize calls with NameServer using a shared mutex.
	// If registrations of class names interleave with registration of factories,
	// then the code will not work properly anyway. Before a factory is registered,
	// the complete class hierarchy must be known.
	
	// Find all the factories that belong to parents of this type.
	std::set<T> ok_to_clobber;
	for (Type parent=0; parent < t; parent++)
	{
		if (_nameServer.isAncestor(parent, t) and methods[parent])
			ok_to_clobber.insert(methods[parent]);
	}

	// Set the factory for t and all children of its type. Be careful
	// not to clobber any factories that might have been previously
	// declared.
	for (Type chi=t; chi < _nameServer.getNumberOfClasses(); chi++)
	{
		if (_nameServer.isAncestor(t, chi) and
		    (nullptr == methods[chi] or
		     ok_to_clobber.end() != ok_to_clobber.find(methods[chi])))
		{
			methods[chi] = fact;
		}
	}
}

void ClassServer::addFactory(Type t, AtomFactory* fact)
{
	std::unique_lock<std::mutex> l(factory_mutex);
	_atomFactory.resize(_nameServer.getNumberOfClasses());
	splice(_atomFactory, t, fact);
}

void ClassServer::addValidator(Type t, Validator* checker)
{
	std::unique_lock<std::mutex> l(factory_mutex);
	_validator.resize(_nameServer.getNumberOfClasses());
	splice(_validator, t, checker);
}

ClassServer::AtomFactory* ClassServer::getFactory(Type t) const
{
	return t < _atomFactory.size() ? _atomFactory[t] : nullptr;
}

ClassServer::Validator* ClassServer::getValidator(Type t) const
{
	return t < _validator.size() ? _validator[t] : nullptr;
}

Handle ClassServer::factory(const Handle& h) const
{
	Handle result = h;

	// If there is a factory, then use it.
	AtomFactory* fact = getFactory(h->get_type());
	if (fact)
		result = (*fact)(h);

	/* Look to see if we have static typechecking to do */
	Validator* checker =
		classserver().getValidator(result->get_type());

	/* Well, is it OK, or not? */
	if (checker and not checker(result))
		throw SyntaxException(TRACE_INFO,
				"Invalid Atom syntax: %s", result->to_string().c_str());

	return result;
}

ClassServer& opencog::classserver()
{
	static std::unique_ptr<ClassServer> instance(new ClassServer(nameserver()));
	return *instance;
}
