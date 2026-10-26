/*
 * =============================================================================
 *  File    : Person.h
 *  Purpose : Abstract base class for every person the system knows about.
 *            Both SACCO members (Member) and staff (StaffUser) are people
 *            with a name and a phone number, so they inherit from Person.
 *
 *  OOP concepts shown here:
 *    - Abstraction : pure virtual functions make Person a "blueprint" only.
 *    - Inheritance : Member and StaffUser extend Person.
 *    - Polymorphism: getRoleTitle()/displayProfile() behave differently
 *                    for each derived class.
 *    - Encapsulation with 'protected': derived classes may use the fields,
 *                    outside code must use the public getters/setters.
 * =============================================================================
 */
#ifndef PERSON_H
#define PERSON_H

#include <string>
#include <iostream>

class Person
{
protected:
    std::string fullName;
    std::string phoneNumber;

public:
    Person();
    Person(const std::string& name, const std::string& phone);
    virtual ~Person();

    std::string getFullName() const;
    std::string getPhoneNumber() const;
    void setFullName(const std::string& name);
    void setPhoneNumber(const std::string& phone);

    // Every kind of person must say what role it plays and how to print itself.
    virtual std::string getRoleTitle() const = 0;
    virtual void displayProfile(std::ostream& out) const = 0;
};

#endif // PERSON_H
