#include "RobotBase.h"
#include <cstdlib>
#include <ctime>
#include <set>
#include <cmath>
#include <limits>
#include <utility>

class Robot_Flame_e_o : public RobotBase 
{
private:
    bool target_found = false;
    int target_row = -1;
    int target_col = -1;

    int radar_direction = 1; 
    bool fixed_radar = false; 
    const int max_range = 4; 
    std::set<std::pair<int, int>> obstacles_memory; 

    int calculate_distance(int row1, int col1, int row2, int col2) const 
    {
        return std::abs(row1 - row2) + std::abs(col1 - col2);
    }


    void find_closest_enemy(const std::vector<RadarObj>& radar_results, int current_row, int current_col) 
    {
        target_found = false;
        int closest_distance = std::numeric_limits<int>::max();

        for (const auto& obj : radar_results) 
        {
            if (obj.m_type == 'R')
            {
                int distance = calculate_distance(current_row, current_col, obj.m_row, obj.m_col);
                if (distance <= max_range && distance < closest_distance) 
                {
                    closest_distance = distance;
                    target_row = obj.m_row;
                    target_col = obj.m_col;
                    target_found = true;
                    fixed_radar = true; 
                }
            }
        }
    }

    void update_obstacle_memory(const std::vector<RadarObj>& radar_results) 
    {
        for (const auto& obj : radar_results) 
        {
            if (obj.m_type == 'M' || obj.m_type == 'P' || obj.m_type == 'F') 
            {
                obstacles_memory.insert({obj.m_row, obj.m_col});
            }
        }
    }

    bool is_passable(int row, int col) const 
    {
        return obstacles_memory.find({row, col}) == obstacles_memory.end();
    }

public:
    Robot_Flame_e_o() : RobotBase(2, 5, flamethrower) 
    {
        std::srand(static_cast<unsigned int>(std::time(nullptr))); 
    }


    virtual void get_radar_direction(int& radar_direction_out) override 
    {
        if (fixed_radar && target_found) 
        {

            radar_direction_out = radar_direction;
        } 
        else 
        {

            radar_direction_out = radar_direction;
            radar_direction = (radar_direction % 8) + 1; 
        }
    }


    virtual void process_radar_results(const std::vector<RadarObj>& radar_results) override 
    {
        target_found = false;
        int current_row, current_col;
        get_current_location(current_row, current_col);

    
        update_obstacle_memory(radar_results);


        find_closest_enemy(radar_results, current_row, current_col);

        if (!target_found) 
        {
            fixed_radar = false; 
        }
    }


    virtual bool get_shot_location(int& shot_row, int& shot_col) override 
    {
        if (target_found) 
        {
            int current_row, current_col;
            get_current_location(current_row, current_col);

            if (calculate_distance(current_row, current_col, target_row, target_col) <= max_range) 
            {
  
                shot_row = target_row;
                shot_col = target_col;
                return true;
            } 
            else 
            {

                target_found = false;
                fixed_radar = false;
            }
        }

        return false; 
    }


    virtual void get_move_direction(int& move_direction, int& move_distance) override 
    {
        int current_row, current_col;
        get_current_location(current_row, current_col);

        if (target_found) 
        {
            int row_step = (target_row > current_row) ? 1 : (target_row < current_row) ? -1 : 0;
            int col_step = (target_col > current_col) ? 1 : (target_col < current_col) ? -1 : 0;

            if (is_passable(current_row + row_step, current_col)) 
            {
                move_direction = (row_step > 0) ? 5 : 1; 
                move_distance = 1;
            } 
            else if (is_passable(current_row, current_col + col_step)) 
            {
                move_direction = (col_step > 0) ? 3 : 7; 
                move_distance = 1;
            } 
            else 
            {
                move_direction = 0;
                move_distance = 0;
            }

            return;
        }

        move_direction = (std::rand() % 8) + 1;
        move_distance = 1;
    }
};


extern "C" RobotBase* create_robot() 
{
    return new Robot_Flame_e_o();
}


extern "C" const char* robot_summary()
{
    return "Scans in cycles, closes in, burns nearby threats.";
}
