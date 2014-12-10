#include "MeanSquaredDisplacement.h"
#include "Trajectory.h"
#include "Frame.h"
#include "TriclinicBox.h"

#include <fstream>
#include <algorithm>

#include <boost/python.hpp>

MeanSquaredDisplacement::MeanSquaredDisplacement(boost::shared_ptr<Trajectory> traj, const std::string& file_name, const unsigned int&  origins)
    : Compute(traj), m_file_name(file_name), m_origins(origins)
    {
    m_type_names.reserve(m_traj->getNumTypes());
    }


void MeanSquaredDisplacement::addType(const std::string& name)
    {
    std::vector<std::string>::iterator name_it = std::find(m_type_names.begin(), m_type_names.end(), name);
    if (name_it == m_type_names.end())
        {
        m_type_names.push_back(name);
        }
    }
void MeanSquaredDisplacement::deleteType(const std::string& name)
    {
    std::vector<std::string>::iterator name_it = std::find(m_type_names.begin(), m_type_names.end(), name);
    if (name_it == m_type_names.end())
        {
        throw std::runtime_error("MeanSquaredDisplacement cannot remove type that doesn't exit");
        }
    else
        {
        m_type_names.erase(name_it);
        }
    }

void MeanSquaredDisplacement::evaluate()
    {

    // read the frames and make sure there is time data
    std::vector< boost::shared_ptr<Frame> > frames = m_traj->getFrames();
    if (!frames[0]->hasTime())
        {
        // error! there is no time data
        throw std::runtime_error("MeanSquaredDisplacement needs data on time");
        }
    // set up the msd
    Vector3< std::vector< std::vector<float> > > msd;
    // zero the msd values and time counter
    unsigned int type_size = std::max((int)m_traj->getNumTypes(),1); // if no types are specified, use all particles
    msd.x.resize(type_size, std::vector<float>( frames.size(), 0.0 ));
    msd.y.resize(type_size, std::vector<float>( frames.size(), 0.0 ));
    msd.z.resize(type_size, std::vector<float>( frames.size(), 0.0 ));

    std::vector<std::string> types = frames[0]->getTypes();

    std::vector<unsigned int> ntime(frames.size(), 0);
    std::vector<unsigned int> time0 ; // vector of time origin frame
    unsigned int t0 = 0;   // time origin counter
    for (unsigned int frame_idx = 0; frame_idx < frames.size(); ++frame_idx)
        {
        if ( ( frame_idx == 0 ) || ( frame_idx % m_origins == 0 ) )
            {
            ++t0 ;
            time0.push_back(t0);
            }

        for ( unsigned int tau = 0; tau < t0; ++tau)        
            {
            // set timestep to match the time origin
            unsigned int delta_t = frame_idx - time0[tau] + 1 ;
            if ( delta_t < frames.size() )
                {
                ntime[delta_t] += 1 ;
                boost::shared_ptr<Frame> cur_frame = frames[delta_t];
                std::vector< Vector3<double> > pos = cur_frame->getPositions();
                boost::shared_ptr<Frame> origin_frame = frames[tau];
                std::vector< Vector3<double> > origin_pos = origin_frame->getPositions();
                for (unsigned int iatom = 0; iatom < m_traj->getN(); ++iatom)
                    {
                    int n_type = m_traj->getTypeByName(types[iatom]);
                    Vector3<double> cur_pos = pos[iatom];
                    Vector3<double> cur_origin_pos = origin_pos[iatom];
                    Vector3<double> diff_pos = cur_pos - cur_origin_pos;

                    msd.x[n_type][delta_t] += ( diff_pos.x * diff_pos.x ) ;
                    msd.y[n_type][delta_t] += ( diff_pos.y * diff_pos.y ) ;
                    msd.z[n_type][delta_t] += ( diff_pos.z * diff_pos.z ) ;
                    } 
                } 
            }
        }



    // output
    for (unsigned int cur_type = 0; cur_type < m_type_names.size(); ++cur_type)
        {
        std::string outf_name = m_file_name + "_" + m_type_names[cur_type] + ".dat";
        std::ofstream outf(outf_name.c_str());
        outf.precision(4);
        outf<<"time msd-total  -x  -y  -z";
        outf<<std::endl;
        for (unsigned int frame_idx = 0; frame_idx < frames.size(); ++frame_idx)
            {
            boost::shared_ptr<Frame> cur_frame = frames[frame_idx];
            double t = cur_frame->getTime(); 
            double norm = ( ntime[frame_idx] ) ;
            outf<<t;
            double msd_tot = ( msd.x[cur_type][frame_idx] + msd.y[cur_type][frame_idx] + msd.z[cur_type][frame_idx] ) ;
            outf<<"\t"<<msd_tot/norm;
            outf<<"\t"<<msd.x[cur_type][frame_idx]/norm;
            outf<<"\t"<<msd.y[cur_type][frame_idx]/norm;
            outf<<"\t"<<msd.z[cur_type][frame_idx]/norm;
            outf<<std::endl;
            }
        outf.close();
        }
    }

void export_MeanSquaredDisplacement()
    {
    using namespace boost::python;
    class_<MeanSquaredDisplacement, boost::shared_ptr<MeanSquaredDisplacement>, bases<Compute>, boost::noncopyable >
    ("MeanSquaredDisplacement", init< boost::shared_ptr<Trajectory>, const std::string&, const unsigned int& >())
    .def("addType",&MeanSquaredDisplacement::addType)
    .def("deleteType",&MeanSquaredDisplacement::deleteType);
    }
    
