#include "RobotBase.h"
#include <iostream>
#include <string>
#include <sstream>



std::ostream& operator<<(std::ostream& os, const WeaponType& weapon)
{
    switch (weapon)
    {
        case flamethrower: os << "flamethrower"; break;
        case railgun:      os << "railgun";      break;
        case grenade:      os << "grenade";      break;
        case hammer:       os << "hammer";       break;
        default:           os << "unknown";      break;
    }

    return os;
}


RobotBase::RobotBase(int move_in, int armor_in, WeaponType weapon_in)
    : m_health(100), m_weapon(weapon_in), m_name("Blank_Robot")
{
    m_grenades = 0;
    if(weapon_in == grenade)
    {
        m_grenades = 15;
    }



    if (move_in < 2)
    {
        m_move = 2;
    }
    else if (move_in > 5)
    {
        m_move = 5;
    }
    else
    {
        m_move = move_in;
    }

    int max_armor = 7 - m_move;


    if (armor_in < 0)
    {
        m_armor = 0;
    }
    else if (armor_in > max_armor)
    {
        m_armor = max_armor;
    }
    else
    {
        m_armor = armor_in;
    }


    m_location_row = 0;
    m_location_col = 0;

}


int RobotBase::get_health()
{
    return m_health;
}

int RobotBase::get_armor()
{
    return m_armor;
}

int RobotBase::get_move_speed()
{
    return m_move;
}

WeaponType RobotBase::get_weapon()
{
    return m_weapon;
}

int RobotBase::get_grenades()
{
    return m_grenades;
}

void RobotBase::decrement_grenades()
{
    m_grenades--;
    if(m_grenades < 0)
        m_grenades = 0;

}

void RobotBase::get_current_location(int& current_row, int& current_col)
{
    current_row = m_location_row;
    current_col = m_location_col;
}


int RobotBase::take_damage(int damage_in)
{
    m_health -= damage_in;
    if (m_health < 0)
    {
        m_health = 0; 
    }
    return m_health;
}

void RobotBase::move_to(int new_row, int new_col)
{
    m_location_row = new_row;
    m_location_col = new_col;
}


void RobotBase::disable_movement()
{
    m_move = 0;
}

void RobotBase::reduce_armor(int amount)
{
    m_armor = m_armor - amount;
    if(m_armor < 0)
        m_armor = 0;

}

void RobotBase::set_boundaries(int row_max, int col_max)
{
    m_board_row_max = row_max;
    m_board_col_max = col_max;
}

std::string RobotBase::print_stats() const {

    std::ostringstream stats;
    stats << m_name << ": ";
    stats << "  H: " << m_health;
    stats << "  W: " << m_weapon;
    stats << "  A: " << m_armor;
    stats << "  M: " << m_move;
    stats << "  at: (" << m_location_row << "," << m_location_col << ") ";

    return stats.str();
}


RobotBase::~RobotBase()
{
    
}