/*
 * Copyright (C) 2024 Emilien Kia <emilien.kia+dev@gmail.com>
 *
 * sqlcpp is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * sqlcpp is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.";
 */

#include "sqlcpp.hpp"

#include <stdexcept>
#include <iostream>

namespace sqlcpp
{

//
// SQLCPP connection creation
//

std::unique_ptr<connection> connection::create(const std::string& connection_string)
{
    return {};
}


//
// SQLCPP resultset row iterator
//

resultset_row_iterator& resultset_row_iterator::operator++()
{
    if(_impl) {
        _impl->next();
    }
    return *this;
}

bool resultset_row_iterator::operator!=(const resultset_row_iterator& other) const
{
    if(!_impl) {
        return !!other._impl;
    } else if(!other._impl) {
        return true;
    } else {
        return _impl->different(*other._impl);
    }
}

row& resultset_row_iterator::operator*()
{
    if(_impl) {
        return _impl->get();
    } else {
        throw std::runtime_error("Invalid iterator");
    }
}

row& resultset_row_iterator::operator->()
{
    if(_impl) {
        return _impl->get();
    } else {
        throw std::runtime_error("Invalid iterator");
    }
}

} // namespace sqlcpp
