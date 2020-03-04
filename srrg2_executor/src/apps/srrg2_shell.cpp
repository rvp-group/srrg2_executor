#include <srrg_boss/json_object_writer.h>
#include <srrg_config/configurable_manager.h>
#include <srrg_config/configurable_visual_shell.h>
#include <srrg_config/linenoise.h>
#include <srrg_system_utils/parse_command_line.h>
#include <srrg_system_utils/system_utils.h>

// ia basic instances
#include <srrg_messages/instances.h>
#include <srrg_messages_ros/instances.h>
#include <srrg_pcl/instances.h>
#include <srrg_slam_interfaces/instances.h>

// ia solver instances
#include <srrg_solver/variables_and_factors/types_2d/instances.h>
#include <srrg_solver/variables_and_factors/types_3d/instances.h>
//#include <srrg_solver/calib/instances.h>
//#include <srrg_solver/linear_solvers/instances.h>
#include <srrg_solver/solver_core/instances.h>

// ia full systems instances
#include <srrg2_proslam_tracking/instances.h>
#include <srrg_laser_slam_2d/instances.h>
//#include <srrg_shaslam_system/instances.h>

// viewer
#include <srrg_qgl_viewport/viewer_core_shared_qgl.h>
#include <srrg_qgl_viewport_ros/viewer_core_ros_qgl.h>
#include <srrg_viewer/drawable_base.h>

//#include <multi_graph_sm/instances.h>

using namespace srrg2_qgl_viewport;
using namespace srrg2_qgl_viewport_ros;
using namespace srrg2_core;
using namespace std;

// ia logging
const std::string app_name("srrg2_shell_app");
#define LOG std::cerr << app_name + "|"

void registerAllInstances() {
  srrg2_core::point_cloud_registerTypes();
  srrg2_core::messages_registerTypes();
  srrg2_core_ros::messages_ros_registerTypes();
  srrg2_solver::solver_registerTypes();
  srrg2_solver::registerTypes2D();
  srrg2_solver::registerTypes3D();
  //srrg2_solver::calib_registerTypes();
  srrg2_laser_tracker_2d::laser_tracker_2d_registerTypes();
  srrg2_slam_interfaces::slam_interfaces_registerTypes();
  srrg2_proslam::srrg2_proslam_tracking_registerTypes();
  //srrg2_shaslam::srrg2_shaslam_registerTypes();
  //  srrg2_slam_architecture::slam_architecture_multi_graph_sm_registerTypes();
}

const char* banner[] = {"dynamic executor", 0};

int main(int argc, char** argv) {
  srrgInit(argc, argv, "shell");
  registerAllInstances();
  Profiler::enable_logging = true;

  ParseCommandLine cmd_line(argv, banner);
  ArgumentString viewer_type(&cmd_line,
                             "vt",
                             "viewer-type",
                             "viewer type; possible choices are <none>, <ros> and <shared>",
                             "shared");
  ArgumentInt viewer_buffer_size(
    &cmd_line,
    "bs",
    "buffer-size-viewer",
    "viewer buffer size if the visualization segfaults means that you have "
    "to increse this number [default is 50MB]",
    BUFFER_SIZE_50MEGABYTE);
  ArgumentString conf_file(&cmd_line, "c", "config-file", "config file to load on start", "");
  cmd_line.parse();
  const std::string exe_name = argv[0];

  StringVector commands;
  if (conf_file.isSet()) {
    std::string open_command = std::string("open ") + conf_file.value();
    commands.push_back(open_command);
  }
  for (auto s : cmd_line.lastParsedArgs()) {
    ifstream is(s.c_str());
    while (is) {
      char buf[1024];
      buf[0] = '#';
      buf[1] = 0;
      is.getline(buf, 1024);
      if (buf[0] && buf[0] != '#')
        commands.push_back(buf);
    }
  }
  commands.push_back(std::string("help"));

  // ia static QApplication to avoid bad thing on the memory
  // ia they should not be shared pointers
  QApplication* qt_server     = nullptr;
  ViewerCoreBase* viewer_core = nullptr;

  if (viewer_type.isSet() || viewer_type.value() != "none") {
    LOG << "viewer requested, creating a qt-server\n";
    qt_server = new QApplication(argc, argv);
  }

  size_t has_gui_name    = std::string(exe_name).find("srrg2_shell_gui");
  size_t has_shared_name = std::string(exe_name).find("srrg2_shell_shared");

  if (viewer_type.value() == "shared" || has_gui_name != std::string::npos) {
    LOG << "creating a shared Qt-based viewer " << std::endl;

    // ia static QApplication to avoid bad thing on the memory
    viewer_core = new ViewerCoreSharedQGL(argc, argv, qt_server, viewer_buffer_size.value());

  } else if (viewer_type.value() == "ros" || has_shared_name != std::string::npos) {
    LOG << "creating a ROS shared Qt-based viewer " << std::endl;
    LOG << "to visualize things, type rosrun srrg2_qgl_viewer_ros "
           "srrg_viewer_ros_client -h in another shell "
        << std::endl;
    LOG << "to discover canvas name type rostopic list (the topic "
           "should be of type [srrg2_core_ros/ViewerBufferMessage])"
        << std::endl;

    viewer_core =
      new ViewerCoreRosQGL(argc, argv, qt_server, "shell_server_node", viewer_buffer_size.value());
  } else {
    LOG << "disabling viewer, single thread execution" << endl;
  }

  // ds allocate a shell with visualization support
  ConfigurableVisualShell shell;
  if (viewer_core) {
    shell.viewer_core = viewer_core;
    viewer_core->stop();
  }

  if (viewer_core) {
    LOG << "starting processing thread\n";
    std::atomic<bool> ready(false);
    std::thread shell_thread(ConfigurableShell::runStatic, commands, &ready);

    while (!ready) {
      if (!ConfigurableShell::instance->_run) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (ConfigurableShell::instance->_run) {
      viewer_core->startViewerServer();
    }
    LOG << "joining processing thread\n";
    shell_thread.join();
  } else {
    LOG << "starting processing in main thread\n";
    shell.run(commands);
  }

  if (viewer_core) {
    LOG << "destroying viewer core\n";
    delete viewer_core;
    viewer_core = nullptr;
  }

  if (qt_server) {
    LOG << "destroying qt server\n";
    delete qt_server;
    qt_server = nullptr;
  }
  return 0;
}
