#include "RobotBase.h"

class Robot_Sniper : public RobotBase
{
private:
    int m_radar_dir = 1;
    int m_target_row = -1;
    int m_target_col = -1;

public:
    Robot_Sniper() : RobotBase(2, 5, railgun)
    {
        m_name = "Sniper";
    }

    void get_radar_direction(int& radar_direction) override
    {
        radar_direction = m_radar_dir;
        m_radar_dir = (m_radar_dir % 8) + 1;
    }

    void process_radar_results(const std::vector<RadarObj>& results) override
    {
        m_target_row = -1;
        m_target_col = -1;
        for (const auto& obj : results)
        {
            if (obj.m_type == 'R')
            {
                m_target_row = obj.m_row;
                m_target_col = obj.m_col;
                return;
            }
        }
    }

    bool get_shot_location(int& shot_row, int& shot_col) override
    {
        if (m_target_row >= 0)
        {
            shot_row = m_target_row;
            shot_col = m_target_col;
            return true;
        }
        return false;
    }

    void get_move_direction(int& direction, int& distance) override
    {
        direction = m_radar_dir;
        distance = 1;
    }
};

extern "C" RobotBase* create_robot()
{
    return new Robot_Sniper();
}

extern "C" const char* robot_summary()
{
    return "Cycles radar, railguns the first robot it sees.";
}
