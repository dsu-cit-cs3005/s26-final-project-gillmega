#pragma once

struct RadarObj 
{

    char m_type;  
    int m_row;   
    int m_col;   


    RadarObj() {} 
    RadarObj(char type, int row, int col) : m_type(type), m_row(row), m_col(col) {}
};