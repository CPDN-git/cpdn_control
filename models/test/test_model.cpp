// test_model.exe
//
// Program to simulate an atmospheric model for testing; based on the OpenIFS model.
//
//    Glenn Carver, CPDN, 2025.
//    Based on earlier work by Andy Bowery, Oxford eResearch Centre, Oxford University.

#include <iostream>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <stdlib.h>
#include <stdio.h>
#include <fstream>
#include <filesystem>
#include <string>
#include <chrono>
#include <thread>

namespace chrono =  std::chrono;
namespace fs = std::filesystem;


int main()
{
    std::string second_part;
    std::string exptid = "NNNN";  // Experiment id must always be exactly 4 characters.
    int timestep = 0;
    int custop = 24;     // max number of timesteps (read from fort.4)

    int nfrres = 0;           // frequency of restarts writes in timesteps
    int nfrpos = 0;           // frequency of model output writes in timesteps
    int utstep = 0;           // model timestep length in seconds (read from fort.4)

    std::cerr << "\n---- Starting test model -----\n";

    // Get the slots path (the current working path)
    fs::path slot_path = std::filesystem::current_path();

    fs::path ifs_stat_file = slot_path / "ifs.stat";
    std::ofstream ifs_stat(ifs_stat_file);

    //--------------------------------------------------
    // Read the model namelist to get the run parameters

    fs::path namelist_file = slot_path / "fort.4";
    std::ifstream namelist(namelist_file);
    std::string namelist_line;

    while ( std::getline(namelist, namelist_line) ) {
         // Parse the namelist lines for the parameters we need
         if ( namelist_line.find("UTSTEP") != std::string::npos ) {
            std::string::size_type sz;
            utstep = std::stoi( namelist_line.substr( namelist_line.find('=') + 1 ), &sz );
            std::cerr << "UTSTEP (timestep) read from fort.4 = " << utstep << '\n';
         }
         if ( namelist_line.find("NFRPOS") != std::string::npos ) {
            std::string::size_type sz;
            nfrpos = std::stoi( namelist_line.substr( namelist_line.find('=') + 1 ), &sz );
            std::cerr << "NFRPOS (output freq in steps) = " << nfrpos << '\n';
         }
         if ( namelist_line.find("NFRRES") != std::string::npos ) {
            std::string::size_type sz;
            nfrres = std::stoi( namelist_line.substr( namelist_line.find('=') + 1 ), &sz );
            std::cerr << "NFRRES (restart write in steps) = " << nfrres << '\n';
         }
         if ( namelist_line.find("CUSTOP") != std::string::npos ) {
            std::string::size_type sz;
            custop = std::stoi( namelist_line.substr( namelist_line.find('=') + 1 ), &sz );
            std::cerr << "CUSTOP (total model steps) = " << custop << '\n';
         }
         if ( namelist_line.find("CNMEXP") != std::string::npos ) {
            exptid = namelist_line.substr( namelist_line.find('=') + 1 );
            exptid.erase(0, exptid.find_first_not_of(" '\"")); // trim leading spaces and quotes
            exptid.erase(exptid.find_last_not_of(" ,'\"") + 1); // trim trailing spaces and quotes
            std::cerr << "EXPTID read from fort.4 = " << exptid << '\n';
         }
    }
    namelist.close();

    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);

    //  Initial sleep to mimic model reading input files before main time loop
    std::this_thread::sleep_until(chrono::system_clock::now() + chrono::seconds(5));

    while (timestep <= custop) {

       std::cerr << "Timestep starting: " << timestep << std::endl;
       
       // Write restarts, only need to create the rcf file for the controller
 
       if ( ( timestep % nfrres ) == 0 && timestep > 0 )
       {
          std::cerr << "Writing restart rcf file at timestep: " << timestep << std::endl;
          // create the rc file, delete if it exists
          fs::path rcf_file = slot_path / "rcf";
          if ( fs::exists( rcf_file ) ) {
             fs::remove( rcf_file );
          }
          // write to rcf file with the value of timestep.
          std::ofstream rcf_file_out(rcf_file);
          rcf_file_out << "&NAMRCF\n" <<
                          "CSTEP=\"" << std::setfill(' ') << std::setw(8) << timestep << "\",\n" <<
                          "CTIME=\"01410000      \",\n" <<
                          "NSTEPLPP=201        ,202        ,49         ,228226     ,228227     ,5*0          ,\n" <<
                          "IPRGPNSRES=1          ,\n" <<
                          "IPRGPEWRES=1          ,\n" <<
                          "IPRTRWRES=1          ,\n" <<
                          "IPRTRVRES=1          ,\n" <<
                          "GMASS0=  98334.671526536738     ,\n" <<
                          "GMASSI=  98334.601637818661     ,\n" <<
                          " /\n" << std::endl;
          rcf_file_out.close();
       }

       // Write out the ICM files on a post-processing step.
       int print_lines = 1;

       if ( (timestep % nfrpos) == 0 )    // output files are also written at step 0.
       {
          print_lines = 3;       // write three lines to ifs.stat for post-processing step (as OpeniFS does)

          std::cerr << "Writing ICM output files at timestep: " << timestep << std::endl;

          std::ostringstream ss;
          ss << exptid << "+" << std::setfill('0') << std::setw(5) << timestep;
          second_part = ss.str();

          fs::path ICMGG_file = slot_path / ("ICMGG" + second_part);
          fs::path ICMSH_file = slot_path / ("ICMSH" + second_part);
          fs::path ICMUA_file = slot_path / ("ICMUA" + second_part);

          // Open the ICM file streams
          std::ofstream ICMGG_file_out(ICMGG_file);
          std::ofstream ICMSH_file_out(ICMSH_file);
          std::ofstream ICMUA_file_out(ICMUA_file);

          // Write to the ICMGG
          for (auto j=0; j<400; j++) {
            ICMGG_file_out << rand() % 10;
          }
          ICMGG_file_out << std::endl;

          // Write to the ICMSH
          for (auto j=0; j<400; j++) {
            ICMSH_file_out << rand() % 10;
          }
          ICMSH_file_out << std::endl;

          // Write to the ICMUA
          for (auto j=0; j<400; j++) {
            ICMUA_file_out << rand() % 10;
          }
          ICMUA_file_out << std::endl;

          // Close the ICM file streams
          ICMGG_file_out.close();
          ICMSH_file_out.close();
          ICMUA_file_out.close();
       }

       // Write to the ifs.stat file each timestep
       for (auto i=0; i < print_lines; i++) {
          std::ostringstream ss;
          ss << " " << std::put_time(&tm, "%H:%M:%S") << " 0AAA00AAA STEPO " << std::setfill(' ') << std::setw(23) << timestep;
          auto output_line = ss.str();
          ifs_stat << output_line << std::endl;
      }

       std::cerr << "Timestep completed: " << timestep << std::endl;
       timestep = timestep + 1;

       // Time delay to slow the program down to allow the main loop of the calling program to run
       std::this_thread::sleep_until(chrono::system_clock::now() + chrono::seconds(10));
    }

    // And finally write the last CNT0 line into the ifs.stat file
    std::ostringstream ss;
    ss << " " << std::put_time(&tm, "%H:%M:%S") << " 0AAA00AAA CNT0 " << std::setfill(' ') << std::setw(22) << timestep;
    auto output_line = ss.str();
    ifs_stat << output_line << std::endl;

    ifs_stat.close();


    // Produce the NODE file
    fs::path NODE_file = slot_path / "NODE.001_01";
    std::ofstream NODE_file_out(NODE_file);
    for (auto j=0; j<400; j++) {
      NODE_file_out << "This is the test NODE file line " << j+1 << '\n';
    }
    NODE_file_out << std::endl;
    NODE_file_out.close();

    std::cerr << "Test model completed successfully\n";

}
