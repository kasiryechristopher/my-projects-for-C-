/*
 * =============================================================================
 *  File    : Person.cpp
 *  Purpose : Implementation of the shared Person base class.
 * =============================================================================
 */
#include "Person.h"

Person::Person() : fullName(""), phoneNumber("")
{
}

Person::Person(const std::string& name, const std::string& phone)
    : fullName(name), phoneNumber(phone)
{
}

Person::~Person()
{
}

std::string Person::getFullName() const
{
    return fullName;
}

std::string Person::getPhoneNumber() const
{
    return phoneNumber;
}

void Person::setFullName(const std::string& name)
{
    if (!name.empty())          // never allow a blank name to be stored
    {
        fullName = name;
    }
}

void Person::setPhoneNumber(const std::string& phone)
{
    if (!phone.empty())
    {
        phoneNumber = phone;
    }
}
