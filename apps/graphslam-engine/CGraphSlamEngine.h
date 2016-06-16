/* +---------------------------------------------------------------------------+
   |                     Mobile Robot Programming Toolkit (MRPT)               |
   |                          http://www.mrpt.org/                             |
   |                                                                           |
   | Copyright (c) 2005-2016, Individual contributors, see AUTHORS file        |
   | See: http://www.mrpt.org/Authors - All rights reserved.                   |
   | Released under BSD License. See details in http://www.mrpt.org/License    |
   +---------------------------------------------------------------------------+ */

// Sun May 22 12:48:25 EEST 2016, nickkouk
#ifndef GRAPHSLAMENGINE_H
#define GRAPHSLAMENGINE_H

#include <mrpt/maps/CSimplePointsMap.h>
#include <mrpt/graphs/CNetworkOfPoses.h>
#include <mrpt/graphslam.h>
#include <mrpt/gui/CBaseGUIWindow.h>
#include <mrpt/gui/CDisplayWindow3D.h>
#include <mrpt/opengl/CPlanarLaserScan.h> // It's in the lib mrpt-maps now
#include <mrpt/opengl/graph_tools.h>
#include <mrpt/opengl/CPointCloud.h>
#include <mrpt/opengl/CRenderizable.h>
#include <mrpt/opengl/CAxis.h>
#include <mrpt/opengl/CCamera.h>
#include <mrpt/opengl/CGridPlaneXY.h>
#include <mrpt/opengl/CSetOfObjects.h>
#include <mrpt/obs/CActionRobotMovement2D.h>
#include <mrpt/obs/CActionRobotMovement3D.h>
#include <mrpt/obs/CObservationOdometry.h>
#include <mrpt/obs/CObservation2DRangeScan.h>
#include <mrpt/obs/CObservationImage.h>
#include <mrpt/obs/CRawlog.h>
#include <mrpt/poses/CPosePDF.h>
#include <mrpt/poses/CPose2D.h>
#include <mrpt/poses/CPose3D.h>
#include <mrpt/slam/CICP.h>
#include <mrpt/slam/CMetricMapBuilder.h>
#include <mrpt/slam/CMetricMapBuilderICP.h>
#include <mrpt/system/filesystem.h>
#include <mrpt/system/datetime.h>
#include <mrpt/system/os.h>
#include <mrpt/system/threads.h>
#include <mrpt/system/string_utils.h>
#include <mrpt/synch/CCriticalSection.h>
#include <mrpt/utils/CLoadableOptions.h>
#include <mrpt/utils/CFileOutputStream.h>
#include <mrpt/utils/CFileInputStream.h>
#include <mrpt/utils/mrpt_stdint.h>
#include <mrpt/utils/mrpt_macros.h>
#include <mrpt/utils/CConfigFile.h>
#include <mrpt/utils/types_simple.h>
#include <mrpt/utils/TColor.h>

#include <string>
#include <sstream>
#include <map>
#include <cerrno>
#include <cmath> // fabs function
#include <set>
#include <algorithm>
#include <cstdlib>

#include "CEdgeCounter.h"
#include "CWindowObserver.h"
#include "CWindowManager.h"
#include "CFixedIntervalsNRD.h"
#include "CICPDistanceERD.h"
#include "supplementary_funs.h"
#include "CWindowObserver.h"


namespace mrpt { namespace graphslam {

	bool verbose = true;
#define VERBOSE_COUT	if (verbose) std::cout << "[graphslam_engine] "

	template< 
  		class GRAPH_t=typename mrpt::graphs::CNetworkOfPoses2DInf,
  		class NODE_REGISTRAR=typename mrpt::graphslam::deciders::CFixedIntervalsNRD_t<GRAPH_t>, 
  		class EDGE_REGISTRAR=typename mrpt::graphslam::deciders::CICPDistanceERD_t<GRAPH_t> >
		class CGraphSlamEngine_t {
			public:

				typedef std::map<std::string, mrpt::utils::CFileOutputStream*> fstreams_out;
				typedef std::map<std::string, mrpt::utils::CFileOutputStream*>::iterator fstreams_out_it;

				typedef std::map<std::string, mrpt::utils::CFileInputStream*> fstreams_in;
				typedef std::map<std::string, mrpt::utils::CFileInputStream*>::iterator fstreams_in_it;

				typedef typename GRAPH_t::constraint_t constraint_t;
				// type of underlying poses (2D/3D)
				typedef typename GRAPH_t::constraint_t::type_value pose_t; 

				typedef mrpt::math::CMatrixFixedNumeric<double,
								constraint_t::state_length, constraint_t::state_length> InfMat;


				// Ctors, Dtors
				//////////////////////////////////////////////////////////////
				CGraphSlamEngine_t(const std::string& config_file,
						mrpt::gui::CDisplayWindow3D* win = NULL,
						CWindowObserver* win_observer = NULL,
						std::string rawlog_fname = "",
						std::string fname_GT = "");
				~CGraphSlamEngine_t();

				// Public function definitions
				//////////////////////////////////////////////////////////////
				/**
		 		 * Wrapper fun around the GRAPH_t corresponding method
		 		 */
				void saveGraph() const {
					MRPT_START;
					assert(m_has_read_config);

					std::string fname = m_output_dir_fname + "/" + m_save_graph_fname;
					saveGraph(fname);

					MRPT_END;
				}
				/**
		 		 * Wrapper fun around the GRAPH_t corresponding method
		 		 */
				void saveGraph(const std::string& fname) const {
					MRPT_START;

					m_graph.saveToTextFile(fname);
					VERBOSE_COUT << "Saved graph to text file: " << fname <<
						" successfully." << std::endl;

					MRPT_END;
				}
				/**
		 		 * Read the configuration file specified and fill in the corresponding
		 		 * class members
		 		 */
	 			void readConfigFile(const std::string& fname);
				/**
		 		 * Print the problem parameters (usually fetched from a configuration
		 		 * file) to the console for verification
		 		 *
		 		 * \sa CGraphSlamEngine_t::parseRawlogFile
		 		 */
				void printProblemParams() const;
				/**
		 		 * Reads the file provided and builds the graph. Method returns false if
		 		 * user issues termination coe (Ctrl+c) otherwise true
		 		 **/
				bool parseRawlogFile();
				/**
		 		 * CGraphSlamEngine_t::optmizeGraph
		 		 *
		 		 * Optimize the under-construction graph
		 		 */
				inline void optimizeGraph(GRAPH_t* graph);
				/**
		 		 * CGraphSlamEngine_t::visualizeGraph
		 		 *
		 		 * Called internally for updating the vizualization scene for the graph
		 		 * building procedure
		 		 */
				inline void visualizeGraph(const GRAPH_t& gr);
				/**
		 		 * GRAPH_t getter function - return reference to own graph
		 		 * Handy function for visualization, printing purposes
		 		 */
				const GRAPH_t& getGraph() const { return m_graph; }

			private:
				// Private function definitions
				//////////////////////////////////////////////////////////////

				/**
		 		 * General initialization method to call from the different Ctors
		 		 */
				void initCGraphSlamEngine();
				/**
		 		 * Initialize (clean up and create new files) the output directory
		 		 * Also provides cmd line arguements for the user to choose the desired
		 		 * action.
		 		 * \sa CGraphSlamEngine_t::initResultsFile
		 		 */
				void initOutputDir();
				/**
		 		 * Method to automate the creation of the output result files
		 		 * Open and write an introductory message using the provided fname
		 		 */
				void initResultsFile(const std::string& fname);
				/**
 				 * updateCurrPosViewport
 				 *
 				 * Udpate the viewport responsible for displaying the graph-building
 				 * procedure in the estimated position of the robot
 				 */
				inline void updateCurrPosViewport(const GRAPH_t& gr);
				// TODO - move it into a map builder template class
				/**
				 * updateMap
				 *
				 * Visualize the estimated path of the robot along with the produced
				 * map
				 */
				void updateMap(const GRAPH_t& gr, 
						std::map<const mrpt::utils::TNodeID, 
						mrpt::obs::CObservation2DRangeScanPtr> m_nodes_to_laser_scans,
						bool full_update=false );
				/**
				 * decimateLaserScan
				 *
				 * Cut down on the size of the given laser scan. Handy for reducing the
				 * size of the resulting CSetOfObject that would be inserted in the
				 * visualization scene
				 */
				 void decimateLaserScan(mrpt::obs::CObservation2DRangeScan* scan,
				 		 int keep_every_n_entries=20);
				/**
		 		 * readGT
		 		 *
		 		 * Parse the ground truth .txt file and fill in the corresponding
		 		 * m_GT_poses vector. Return true if operation was successful Call the
		 		 * function in the constructor if the visualize_GT flag is set
		 		 * to true. 
		 		 */
				inline void readGT(const std::string& rawlog_fname_GT);
				/**
				 * updateGTVisualization
				 *
				 * Display the next ground truth position in the visualization window
				 */
				void updateGTVisualization();
				/**
		 		 * autofitObjectInView
		 		 *
		 		 * Set the camera parameters of the CDisplayWindow3D so that the whole
		 		 * graph is viewed in the window.
		 		 */
				inline void autofitObjectInView(const mrpt::opengl::CSetOfObjectsPtr& gr);
				/**
		 		 * queryObserverForEvents
		 		 *
		 		 * Query the given observer for any events (keystrokes, mouse clicks,
		 		 * that may have occured in the CDisplayWindow3D  and fill in the
		 		 * corresponding class variables
		 		 */
				inline void queryObserverForEvents();

				// VARIABLES
				//////////////////////////////////////////////////////////////

				// the graph object to be built and optimized
				GRAPH_t m_graph;

				// registrator instances
				NODE_REGISTRAR m_node_registrar; 
				EDGE_REGISTRAR m_edge_registrar; 

				/**
		 		 * Problem parameters.
		 		 * Most are imported from a .ini config file
		 		 * \sa CGraphSlamEngine_t::readConfigFile
		 		 */
				std::string	m_config_fname;

				std::string	m_rawlog_fname;
				std::string	m_fname_GT;
				size_t m_curr_GT_poses_index; // counter for reading back the GT_poses
				std::string	m_output_dir_fname;
				bool m_user_decides_about_output_dir;
				std::string	m_save_graph_fname;

				bool m_do_pose_graph_only;
				std::string	m_optimizer;

				bool m_has_read_config;
				bool m_observation_only_rawlog;

		 		// keeps track of the out fstreams so that they can be
		 		// closed (if still open) in the class Dtor.
				fstreams_out m_out_streams;

				// visualization objects
				mrpt::gui::CDisplayWindow3D* m_win;
				CWindowObserver* m_win_observer;
				mrpt::gui::CWindowManager_t m_win_manager;

				// Interaction with the CDisplayWindow - use of CWindowObserver
				bool m_autozoom_active, m_request_to_exit;

				mrpt::utils::TParametersDouble m_optimized_graph_viz_params;
				bool m_visualize_optimized_graph;
				bool m_visualize_odometry_poses;
				bool m_visualize_GT;
				bool m_visualize_map;

				// textMessage vertical text position
				double m_curr_offset_y;
				double m_offset_y_graph;
				double m_offset_y_odometry;
				double m_offset_y_timestamp;
				double m_offset_y_GT;

				// textMessage index
				int m_curr_text_index;
				int m_text_index_graph;
				int m_text_index_odometry;
				int m_text_index_timestamp;
				int m_text_index_GT;

				// instance to keep track of all the edges + visualization related
				// functions
				CEdgeCounter_t m_edge_counter;
				int m_num_of_edges_for_collapse;

				// std::maps to store information about the graph(s)
				std::map<const GRAPH_t*, std::string> graph_to_name;
				std::map<const GRAPH_t*, mrpt::utils::TParametersDouble*> graph_to_viz_params;

				// pose_t vectors
				std::vector<pose_t*> m_odometry_poses;
				std::vector<pose_t*> m_GT_poses;

				std::map<const mrpt::utils::TNodeID, 
					mrpt::obs::CObservation2DRangeScanPtr> m_nodes_to_laser_scans;
				mrpt::obs::CObservation2DRangeScanPtr m_last_laser_scan;


				// PointCloud colors
				mrpt::utils::TColorf m_odometry_color; // see Ctor for initialization
				mrpt::utils::TColorf m_GT_color;
				mrpt::utils::TColor m_robot_model_color;

				bool m_is3D;
				// internal counter for querrying for the number of nodeIDs.
				// Handy for not locking the m_graph resource
				mrpt::utils::TNodeID m_nodeID_max; 

				// graph optimization
				mrpt::utils::TParametersDouble m_optimization_params;
				pose_t m_curr_estimated_pose;

				// Use mutliple threads for visualization and graph optimization
				mrpt::system::TThreadHandle m_thread_optimize;
				mrpt::system::TThreadHandle m_thread_visualize;

				// mark graph modification/accessing explicitly for multithreaded
				// implementation
				mrpt::synch::CCriticalSection m_graph_section;

		};

} } // end of namespaces
// pseudo-split the definition and implementation of template
#include "CGraphSlamEngine_impl.h"

#endif /* end of include guard: GRAPHSLAMENGINE_H */
