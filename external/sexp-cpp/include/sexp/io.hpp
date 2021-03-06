// SExp - A S-Expression Parser for C++
// Copyright (C) 2006 Matthias Braun <matze@braunis.de>
//               2015 Ingo Ruhnke <grumbel@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef HEADER_SEXP_IO_HPP
#define HEADER_SEXP_IO_HPP

#include <ostream>

#include <sexp/value.hpp>

#ifdef SEXP_DLL
#define SEXP_API __declspec( dllexport )  
#else
#define SEXP_API __declspec( dllimport )
#endif

namespace sexp {

SEXP_API void escape_string(std::ostream& os, std::string const& text);

SEXP_API std::ostream& operator<<(std::ostream& os, Value const& sx);

} // namespace sexp

#endif

/* EOF */
