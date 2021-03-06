/*! 
 * \file GROTrajectory.cc
 * \author Sang Beom Kim
 * \author Michael P. Howard
 * \date 29 December 2014
 * \brief Implementation of GROTrajectory reader
 */
#include "GROTrajectory.h"

#include <boost/algorithm/string.hpp>
#include <boost/python.hpp>
#include <fstream>
#include <sstream>
#include <cstdlib>

/*!
 * \param precision number of decimal places in the positions
 */
GROTrajectory::GROTrajectory(unsigned int precision)
    : m_n_gro_digits(precision+5)
    {
    m_gro_line_length = 21 + 5*m_n_gro_digits; // 4x5 + 5*(chars in pos/vel) + 1 = minimum
    }

/*!
 * Opens and loops over all attached files and calls readFromFile on them.
 * Each file may contain multiple frames inside
 */
void GROTrajectory::read()
    {
    for (unsigned int cur_f = 0; cur_f < m_files.size(); ++cur_f)
        {
        // open GRO file
        std::ifstream file(m_files[cur_f].c_str());
        if (!file.good())
            {
            throw std::runtime_error("GROTrajectory: cannot find GRO file " + m_files[cur_f]);
            }
        readFromFile(file);
        file.close();
        }
    m_must_read_from_file = false;
    }

/*! 
 * \param file string stream of current file to parse
 *
 * All file lines are looped over, ignoring white space, until a comment line is found that *must* contain
 * the timestep (t= <time>).
 */
void GROTrajectory::readFromFile(std::ifstream& file)
    {
    // string container for parsing line by line
    std::string line;
    
    while (getline(file, line))
        {
        // skip over empty lines until we find a comment line
        if (line.empty())
            continue;
            
        std::istringstream line_parser;
        
        // extract time step from comment line
        double time_step(0.0);
        int found_time = line.find("t=");
        if (found_time >= 0 && (found_time+2) < (int)line.length())
            {
            line_parser.str(line.substr(found_time+2));
            line_parser.clear();
            if (!(line_parser >> time_step))
                {
                throw std::runtime_error("GROTrajectory: time step must follow t=");
                }
            }
        else
            {
            throw std::runtime_error("GROTrajectory: time step is required in comment line");
            }
            
        // extract the number of atoms and construct the frame
        boost::shared_ptr<Frame> cur_frame;
        if (getline(file,line))
            {
            line_parser.str(line);
            line_parser.clear();
            unsigned int n_particles(0);
            if (line_parser >> n_particles)
                {
                cur_frame = boost::shared_ptr<Frame>( new Frame(n_particles) );
                cur_frame->setTime(time_step);
                }
            else
                {
                throw std::runtime_error("GROTrajectory: number of particles must be set");
                }
            }
        else
            {
            throw std::runtime_error("GROTrajectory: number of particles must be set");
            }
            
        // loop on particles now
        unsigned int cur_particle(0);
        bool auto_number = false;
        while (cur_particle < cur_frame->getN() && getline(file,line))
            {
            std::string name_i;
            int particle_id(-1);
            Vector3<double> pos_i, vel_i;
            readLine(line,name_i,particle_id,pos_i,vel_i);
            
            // gro is indexed 1 to N, so must decrement by 1
            if (!auto_number && (particle_id > 0 && particle_id <= (int)cur_frame->getN()))
                {
                --particle_id;
                }
            else if (auto_number || (cur_particle == 0 && particle_id == 0))
                {
                // auto numbering can only be switched on from the first particle
                // switch on auto numbering if labels are not present
                particle_id = cur_particle;
                auto_number = true;
                // should warn the user
                }
            else
                {
                throw std::runtime_error("GROTrajectory: particle ids run 1 to N");
                }
            
            if (name_i.length() > 0)
                {
                cur_frame->setName(particle_id, name_i);
                }
            
            // set particle position and velocity
            cur_frame->setPosition(particle_id, pos_i);
            cur_frame->setVelocity(particle_id, vel_i);
            
            ++cur_particle;
            }
        if (cur_particle < cur_frame->getN())
            {
            throw std::runtime_error("GROTrajectory: number of particles read does not match specified number");
            }
        
        // acquire the simulation box
        // gro uses a funny ordering:
        // v1(x) v2(y) v3(z) v1(y) v1(z) v2(x) v2(z) v3(x) v3(z)
        // and always requires the first three be present
        if (getline(file, line))
            {
            line_parser.str(line);
            line_parser.clear();
            
            Vector3<double> v1,v2,v3;
            if( !(line_parser >> v1.x >> v2.y >> v3.z ) )
                {
                throw std::runtime_error("GROTrajectory: box must be specified");
                }
            
            // nested check for the quantities, since we only need to check for more if they exist
            if (line_parser >> v1.y)
                {
                if (line_parser >> v1.z)
                    {
                    if (line_parser >> v2.x)
                        {
                        if (line_parser >> v2.z)
                            {
                            if (line_parser >> v3.x)
                                {
                                if (line_parser >> v3.z)
                                    {
                                    // we have all the values now, stop
                                    }
                                }
                            }
                        }
                    }
                }
            
            // now construct the box using three arbitrarily oriented lattice vectors
            TriclinicBox box(v1,v2,v3);
            cur_frame->setBox(box);
            }
        else
            {
            throw std::runtime_error("GROTrajectory: box must be specified");
            }
        m_frames.push_back(cur_frame);
        }
    }

inline void GROTrajectory::readLine(const std::string& line,
                                    std::string& name,
                                    int& particle_id,
                                    Vector3<double>& pos,
                                    Vector3<double>& vel) const
    {
    
    if (line.length() < m_gro_line_length)
        {
        throw std::runtime_error("GROTrajectory: particle line does not adhere to minimum gro fomatting");
        }
    
    // extract the particle data using the fixed column gro format
    name = line.substr(10,5);
    boost::algorithm::trim(name);
    
    particle_id = atoi(line.substr(15,5).c_str());
            
    unsigned int extract = 20;
    pos.x = strtod(line.substr(extract, m_n_gro_digits).c_str(), NULL);
    extract += m_n_gro_digits;
    
    pos.y = strtod(line.substr(extract, m_n_gro_digits).c_str(), NULL);
    extract += m_n_gro_digits;
    
    pos.z = strtod(line.substr(extract, m_n_gro_digits).c_str(), NULL);
    extract += m_n_gro_digits;
    
    vel.x = strtod(line.substr(extract, m_n_gro_digits).c_str(), NULL);
    extract += m_n_gro_digits;
    
    vel.y = strtod(line.substr(extract, m_n_gro_digits).c_str(), NULL);
    extract += m_n_gro_digits;
    
    vel.z = strtod(line.substr(extract, m_n_gro_digits).c_str(), NULL);
    }

void export_GROTrajectory()
    {
    using namespace boost::python;
    class_<GROTrajectory, boost::shared_ptr<GROTrajectory>, bases<Trajectory>, boost::noncopyable>
    ("GROTrajectory", init<unsigned int>());
    }
