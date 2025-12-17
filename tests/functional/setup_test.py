#! /usr/bin/python3

# Script to setup the file structure required to run a test of new checked in code using GitHub Action 
# or run locally.  This test mimics a workunit for the OpenIFS 43r3 model.

#   Glenn Carver, CPDN Development Team, Nov 2025.
#   Based on earlier version by Andy Bowery.

if __name__ == "__main__":

    import os, secrets, zipfile, shutil
    import sys, json
    from pathlib import Path

    def write_file(path: Path, content: str):
        path.write_text(content)

    def zip_single_file(src: Path, dst: Path | None = None, arcname: str | None = None):
        """Zip one file into dst (defaults to src+'.zip')."""
        dst = dst or src.with_name(src.name + ".zip")
        with zipfile.ZipFile(dst, "w") as zf:
            zf.write(src, arcname=arcname or src.name)

    def ensure_dir(path: Path):
        if path.exists() or path.is_symlink():
            if path.is_dir() and not path.is_symlink():
                shutil.rmtree(path)
            else:
                path.unlink()
        path.mkdir(exist_ok=True)

    if len(sys.argv) < 2:
        print("Usage: setup_test.py <config.json>")
        sys.exit(1)
    
    # Load the test configuration file.
    # A small json file with the test parameters.
    with open(sys.argv[1]) as f:
        config = json.load(f)
    print(f"[setup] Loaded config from {sys.argv[1]}: {config.get('test_name', '<unnamed>')}")

    forecast_length = config["forecast_length"]
    experiment_id = config["experiment_id"]
    unique_member_id = config["unique_member_id"]
    batch_id = config["batch_id"]
    upload_interval = config["upload_interval"]
    timestep = float(config["timestep"])
    nfrres = config["nfrres"]
    nfrpos = config["nfrpos"]

    # Make sure we're in the 'test' directory, if not then exit with error
    current_path = Path.cwd()
    print(f"[setup] Running in directory: {current_path}")

    projects_dir = current_path / "projects"
    ensure_dir(projects_dir)
    print(f"[setup] Ensured projects dir: {projects_dir}")

    slots_dir = current_path / "slots"
    ensure_dir(slots_dir)
    slot0_dir = slots_dir / "0"
    ensure_dir(slot0_dir)
    print(f"[setup] Ensured slot dir: {slot0_dir}")

    # Produce fake test_app file in projects directory
    # Fake because we put the test_model exe directly in the slot
    test_app = projects_dir / "test_model_app_1.00_x86_64-pc-linux-gnu"
    with open(test_app, 'a') as test_app_file:
      test_app_file.write(secrets.token_hex(4000) + '\n')
    zip_single_file(test_app)
    os.remove(test_app)   # remove unzipped version
    print(f"[setup] Created fake app package: {test_app.name}.zip")

    # Create the init_data.xml, needed by BOINC
    init_data_string = "   <app_init_data>\n" +\
                       "     <major_version>1</major_version>\n" +\
                       "     <minor_version>0</minor_version>\n" +\
                       "     <release>1</release>\n" +\
                       "     <app_version>100</app_version>\n" +\
                       "     <hostid>0</hostid>\n" +\
                       "     <app_name>test_model</app_name>\n" +\
                       "     <project_preferences></project_preferences>\n" +\
                       f"     <project_dir>{projects_dir}</project_dir>\n" +\
                       f"     <boinc_dir>{current_path}</boinc_dir>\n" +\
                       f"     <wu_name>oifs_43r3_{unique_member_id}_yyyymmddhh_1_{batch_id}_0</wu_name>\n" +\
                       "     <shm_key>0</shm_key>\n" +\
                       "     <slot>0</slot>\n" +\
                       "     <wu_cpu_time>0.000000</wu_cpu_time>\n" +\
                       "     <starting_elapsed_time>0.000000</starting_elapsed_time>\n" +\
                       "     <user_total_credit>0.000000</user_total_credit>\n" +\
                       "     <user_expavg_credit>0.000000</user_expavg_credit>\n" +\
                       "     <host_total_credit>0.000000</host_total_credit>\n" +\
                       "     <host_expavg_credit>0.000000</host_expavg_credit>\n" +\
                       "     <resource_share_fraction>0.000000</resource_share_fraction>\n" +\
                       "     <checkpoint_period>60.000000</checkpoint_period>\n" +\
                       "     <fraction_done_start>0.000000</fraction_done_start>\n" +\
                       "     <fraction_done_end>1.000000</fraction_done_end>\n" +\
                       "     <rsc_fpops_est>0.000000</rsc_fpops_est>\n" +\
                       "     <rsc_fpops_bound>0.000000</rsc_fpops_bound>\n" +\
                       "     <rsc_memory_bound>0.000000</rsc_memory_bound>\n" +\
                       "     <rsc_disk_bound>0.000000</rsc_disk_bound>\n" +\
                       "     <computation_deadline>0.000000</computation_deadline>\n" +\
                       "     <host_info></host_info>\n" +\
                       "     <proxy_info></proxy_info>\n" +\
                       "     </global_preferences>\n" +\
                       "   </app_init_data>\n"

    init_data_path = slot0_dir / "init_data.xml"
    write_file(init_data_path, init_data_string)
    print(f"[setup] Wrote {init_data_path.name}")


    # Create the fake model namelist file, fort.4.
    # TODO: TRICKLE_UPLOAD_FREQUENCY and UPLOAD_NUMBER can be removed as 
    # they are not used by the controller code any more.
    # TODO: Should not have TSTEP as well as UTSTEP here, use the namelist variable always!

    # Explanation of variable values in fort.4:
    # Values below are populated from the test config JSON:
    # EXPTID = Dummy experiment ID
    # UNIQUE_MEMBER_ID = 1353 : Dummy workunit id
    # HORIZ_RESOLUTION = 159 : Horizontal resolution l159
    # VERT_RESOLUTION = 91 : Vertical resolution 91 levels
    # GRID_TYPE = l_2 : Reduced Gaussian grid type l_2
    # UPLOAD_INTERVAL = Upload interval in model steps
    # UTSTEP = Model time step in seconds
    # CUSTOP = Total number of model time steps to run
    # CNMEXP = Dummy experiment ID
    # NFRRES = Frequency of restart file output in model time steps
    # NFRPOS = Frequency of post-processed output in model time steps

    fort_file_string = "!WU_TEMPLATE_VERSION=43r3-seasonal-20250801\n"+\
                         f"!EXPTID={experiment_id}\n"+\
                         f"!UNIQUE_MEMBER_ID={unique_member_id}\n"+\
                         "!IC_ANCIL_FILE=ic_ancil_0\n" +\
                         "!IFSDATA_FILE=ifsdata_0\n" +\
                         "!CLIMATE_DATA_FILE=clim_data_0\n" +\
                         "!HORIZ_RESOLUTION=159\n" +\
                         "!VERT_RESOLUTION=91\n" +\
                         "!GRID_TYPE=l_2\n" +\
                         f"!UPLOAD_INTERVAL={upload_interval}\n" +\
                         "\n\n"+\
                         "&NAMARG\n"+\
                         f" UTSTEP={timestep:.1f},\n" +\
                         f" CUSTOP={forecast_length},\n" +\
                         f" CNMEXP='{experiment_id}',\n" +\
                         "/\n"+\
                         "&NAMRES\n"+\
                         f" NFRRES={nfrres},\n"+\
                         "/\n"+\
                         "&NAMCT0\n"+\
                         f" NFRPOS={nfrpos},\n" +\
                         "/\n"

    fort4_path = slot0_dir / "fort.4"
    write_file(fort4_path, fort_file_string)
    zip_single_file(fort4_path, slot0_dir / "jf_namelist", arcname="fort.4")
    os.remove(fort4_path)   # remove unzipped version
    print(f"[setup] Wrote fort.4 and jf_namelist")

    # The OpenIFS BOINC implementation uses mapped logical filenames to
    # identify the various input files.  Here we create the files with
    # the logical names expected by the model.
    # It is less than ideal as it makes the code more complex to handle
    # resolving these files. It also makes it harder to debug as the
    # filenames are not descriptive.

    # Create logical namelist file
    namelist_string = ">jf_namelist<\n"
    namelist_path = slot0_dir / f"test_model_{unique_member_id}_yyyymmddhh_1_{batch_id}_0.zip"
    write_file(namelist_path, namelist_string)
    print(f"[setup] Wrote logical namelist {namelist_path.name}")

    # Create ic_ancil file
    ic_ancil_string = ">jf_ic_ancil<\n"
    write_file(slot0_dir / "ic_ancil_0.zip", ic_ancil_string)
    print("[setup] Wrote ic_ancil_0.zip")

    # Produce jf_ic_ancil file
    jf_ic_ancil_path = slot0_dir / 'jf_ic_ancil'
    with open(jf_ic_ancil_path, 'a') as jf_ic_ancil_file:
      jf_ic_ancil_file.write(secrets.token_hex(4000) + '\n')
    jf_ic_ancil_zip = slot0_dir / 'jf_ic_ancil.zip'
    zip_single_file(jf_ic_ancil_path, jf_ic_ancil_zip, arcname='jf_ic_ancil')
    shutil.move(jf_ic_ancil_zip, jf_ic_ancil_path)
    print("[setup] Created jf_ic_ancil logical file")

    # Create ifsdata file
    ifsdata_string = ">jf_ifsdata<\n"
    write_file(slot0_dir / "ifsdata_0.zip", ifsdata_string)
    print("[setup] Wrote ifsdata_0.zip")

    # Produce jf_ifsdata file
    jf_ifsdata_path = slot0_dir / 'jf_ifsdata'
    with open(jf_ifsdata_path, 'a') as jf_ifsdata_file:
      jf_ifsdata_file.write(secrets.token_hex(4000) + '\n')
    jf_ifsdata_zip = slot0_dir / 'jf_ifsdata.zip'
    zip_single_file(jf_ifsdata_path, jf_ifsdata_zip, arcname='jf_ifsdata')
    shutil.move(jf_ifsdata_zip, jf_ifsdata_path)
    print("[setup] Created jf_ifsdata logical file")

    # Create clim_data file
    clim_data_string = ">jf_clim_data<\n"
    write_file(slot0_dir / "clim_data_0.zip", clim_data_string)
    print("[setup] Wrote clim_data_0.zip")

    # Produce jf_clim_data file
    jf_clim_data_path = slot0_dir / 'jf_clim_data'
    with open(jf_clim_data_path, 'a') as jf_clim_data_file:
      jf_clim_data_file.write(secrets.token_hex(4000) + '\n')
    jf_clim_data_zip = slot0_dir / 'jf_clim_data.zip'
    zip_single_file(jf_clim_data_path, jf_clim_data_zip, arcname='jf_clim_data')
    shutil.move(jf_clim_data_zip, jf_clim_data_path)
    print("[setup] Created jf_clim_data logical file")

    print("[setup] Test fixture generation complete")
