//***************************************************************************
//* Copyright (c) 2011-2012 Saint-Petersburg Academic University
//* All Rights Reserved
//* See file LICENSE for details.
//****************************************************************************

/*
 * Assembler Main
 */
#include "standard.hpp"
#include "logger/log_writers.hpp"
#include "segfault_handler.hpp"
#include "stacktrace.hpp"
#include "config_struct.hpp"
#include "io/easy_reader.hpp"
#include "io/rc_reader_wrapper.hpp"
#include "io/cutting_reader_wrapper.hpp"
#include "io/multifile_reader.hpp"
#include "io/careful_filtering_reader_wrapper.hpp"
#include "launch.hpp"
#include "simple_tools.hpp"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "omni/distance_estimation.hpp"
#include "memory_limit.hpp"
#include "read_converter.hpp"

#include "perfcounter.hpp"

#include "runtime_k.hpp"

void link_output(std::string const& link_name)
{
    if (!cfg::get().run_mode)
        return;

    std::string link = cfg::get().output_root + link_name;
    unlink(link.c_str());
    if (symlink(cfg::get().output_suffix.c_str(), link.c_str()) != 0)
        WARN( "Symlink to \"" << link << "\" launch failed");
}

void link_previous_run(std::string const& previous_link_name, std::string const& link_name){
   if (!cfg::get().run_mode)
       return;

   char buf[255];

   std::string link = cfg::get().output_dir + previous_link_name;
   unlink(link.c_str());
   int count = readlink((cfg::get().output_root + link_name).c_str(), buf, sizeof(buf) - 1);
   if (count >= 0){
       buf[count] = '\0';
       std::string previous_run("../");
       previous_run = previous_run + buf;
       if (symlink(previous_run.c_str(), link.c_str()) != 0) {
           DEBUG( "Symlink to \"" << link << "\" launch failed : " << previous_run);
       }
   } else {
	   DEBUG( "Symlink to \"" << link << "\" launch failed");
   }
}

struct on_exit_output_linker
{
    on_exit_output_linker(std::string const& link_name) :
            link_name_(link_name)
    {
    }

    ~on_exit_output_linker()
    {
        link_previous_run("previous", link_name_);
        link_output(link_name_);
    }

private:
    std::string link_name_;
};

void copy_configs(string cfg_filename, string to)
{
    using namespace debruijn_graph;

    if (!make_dir(to)) {
    	WARN("Could not create files use in /tmp directory");
    }
    copy_files_by_ext(path::parent_path(cfg_filename), to, ".info", true);
}

void load_config(string cfg_filename)
{
    
    checkFileExistenceFATAL(cfg_filename);

    // deprecated: precopy of config in /tmp (!!!) just to read the values of output_root and output_dir!
//    fs::path tmp_folder = fs::path("/tmp") / debruijn_graph::MakeLaunchTimeDirName() / ("K");
//    copy_configs(cfg_filename, tmp_folder);
//    cfg_filename = (tmp_folder / fs::path(cfg_filename).filename()).string();
    
    cfg::create_instance(cfg_filename);

    make_dir(cfg::get().output_root);

    make_dir(cfg::get().output_dir);
    if (cfg::get().make_saves) {
        make_dir(cfg::get().output_saves);
    }

    make_dir(cfg::get().temp_bin_reads_path);

    string path_to_copy = path::append_path(cfg::get().output_dir, "configs");
        copy_configs(cfg_filename, path_to_copy);
    }

void create_console_logger(string cfg_filename) {
  using namespace logging;

  string log_props_file = cfg::get().log_filename;

  if (!fileExists(log_props_file))
    log_props_file = path::append_path(path::parent_path(cfg_filename), cfg::get().log_filename);

  logger *lg = create_logger(fileExists(log_props_file) ? log_props_file : "");
  lg->add_writer(std::make_shared<console_writer>());
  attach_logger(lg);
}

int main(int argc, char** argv)
{
	//BOOST_STATIC_ASSERT(debruijn_graph::K % 2 != 0);

	perf_counter pc;
    
    const size_t GB = 1 << 30;
    
    segfault_handler sh(bind(link_output, "latest"));
    
    try
    {
        using namespace debruijn_graph;

        string cfg_filename = argv[1];

        load_config          (cfg_filename);
        create_console_logger(cfg_filename);

        on_exit_output_linker try_linker("latest");

        VERIFY(cfg::get().K >= runtime_k::MIN_K && cfg::get().K < runtime_k::MAX_K);
        VERIFY(cfg::get().K % 2 != 0);

        // read configuration file (dataset path etc.)

        // typedefs :)
//        typedef io::EasyReader ReadStream;
//        typedef io::PairedEasyReader PairedReadStream;

        limit_memory(cfg::get().max_memory * GB);

//        if (cfg::get().use_multithreading) {
//            debruijn_graph::convert_reads_to_binary();
//        }

        // assemble it!
        INFO("Assembling " << cfg::get().dataset_name << " dataset (" << cfg::get().dataset_file << ")");
        INFO("with K=" << cfg::get().K);

        debruijn_graph::assemble_genome();

        link_output("latest_success");

        INFO("Assembling " << cfg::get().dataset_name << " dataset with K=" << cfg::get().K << " finished");

    }
    catch (std::bad_alloc const& e)
    {
    	std::cerr << "Not enough memory to run SPAdes. " << e.what() << std::endl;
    	return EINTR;
    }
    catch (std::exception const& e)
    {
        std::cerr << "Exception caught " << e.what() << std::endl;
        return EINTR;
    }
    catch (...)
    {
        std::cerr << "Unknown exception caught " << std::endl;
        return EINTR;
    }
    int ms = int(pc.time_ms());
    int secs = (ms / 1000) % 60;
    int mins = (ms / 1000 / 60) % 60;
    int hours = (ms / 1000 / 60 / 60);
    INFO("Assembling time: " << hours << " hours " << mins << " minutes " << secs << " seconds");

    // OK
    return 0;
}

