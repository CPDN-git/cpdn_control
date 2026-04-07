#! /usr/bin/python3

# Script to setup the file structure required to run a test of new checked in code using GitHub Action 
# or run locally.  This test mimics a workunit for the OpenIFS 43r3 model.

#   Glenn Carver, CPDN Development Team, Nov 2025.
#   Based on earlier version by Andy Bowery.

if __name__ == "__main__":

    import hashlib
    import os, secrets, zipfile, shutil, platform
    import sys, json
    from pathlib import Path
    from typing import Optional

    def detect_platform() -> str:
        env_platform = os.environ.get("CPDN_PLATFORM")
        if env_platform:
            return env_platform

        system = platform.system()
        machine = platform.machine().lower()

        if system == "Darwin":
            arch = "arm64" if "arm" in machine else "x86_64"
            return f"{arch}-apple-darwin"
        if system == "Windows":
            return "x86_64-pc-windows-msvc"
        arch = "aarch64" if "aarch64" in machine or "arm64" in machine else "x86_64"
        return f"{arch}-pc-linux-gnu"

    def write_file(path: Path, content: str):
        path.write_text(content, encoding="utf-8")

    def zip_single_file(src: Path, dst: Optional[Path] = None, arcname: Optional[str] = None):
        """Zip one file into dst (defaults to src+'.zip')."""
        dst = dst or src.with_name(src.name + ".zip")
        with zipfile.ZipFile(dst, "w") as zf:
            zf.write(src, arcname=arcname or src.name)

    def md5_hex(path: Path) -> str:
        digest = hashlib.md5()
        with path.open("rb") as handle:
            for chunk in iter(lambda: handle.read(8192), b""):
                digest.update(chunk)
        return digest.hexdigest()

    def create_project_zip(src: Path, project_dir: Path, arcname: Optional[str] = None) -> Path:
        temp_zip = project_dir / f"{src.name}.zip.tmp"
        zip_single_file(src, temp_zip, arcname=arcname)
        jf_path = project_dir / f"jf_{md5_hex(temp_zip)}"
        if jf_path.exists():
            jf_path.unlink()
        temp_zip.replace(jf_path)
        return jf_path

    def write_soft_link(path: Path, target: Path):
        write_file(path, f"<soft_link>{target.as_posix()}</soft_link>\n")

    def ensure_dir(path: Path):
        if path.exists() or path.is_symlink():
            if path.is_dir() and not path.is_symlink():
                shutil.rmtree(path)
            else:
                path.unlink()
        path.mkdir(exist_ok=True)

    def ensure_not_repo_root(workdir: Path):
        repo_root = Path(__file__).resolve().parents[2]
        if workdir.resolve() == repo_root:
            print(
                f"[setup] Refusing to create projects/ and slots/ in the repository root: {repo_root}",
                file=sys.stderr,
            )
            print("[setup] Run the functional harness from a dedicated test work directory.", file=sys.stderr)
            sys.exit(2)

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
    member_id = config["member_id"]
    batch_id = config["batch_id"]
    wu_name = config["wu_name"]
    upload_interval = config["upload_interval"]
    timestep = float(config["timestep"])
    nfrres = config["nfrres"]
    nfrpos = config["nfrpos"]

    current_path = Path.cwd()
    ensure_not_repo_root(current_path)
    print(f"[setup] Running in directory: {current_path}")

    platform_triplet = detect_platform()

    projects_root = current_path / "projects"
    ensure_dir(projects_root)
    project_dir = projects_root / "climateprediction.net"
    ensure_dir(project_dir)
    print(f"[setup] Ensured project dir: {project_dir}")

    slots_dir = current_path / "slots"
    ensure_dir(slots_dir)
    slot0_dir = slots_dir / "0"
    ensure_dir(slot0_dir)
    print(f"[setup] Ensured slot dir: {slot0_dir}")

    # Produce fake test_app file in projects directory
    # Fake because we put the test_model exe directly in the slot
    test_app = project_dir / f"test_model_app_1.00_{platform_triplet}"
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
                       f"    <project_dir>{project_dir}</project_dir>\n" +\
                       f"    <boinc_dir>{current_path}</boinc_dir>\n" +\
                       f"    <wu_name>test_model_{member_id}_yyyymmddhh_1_{batch_id}_0</wu_name>\n" +\
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

    fort_file_string = "&NAMFPC\n"+\
                         " CFPFMT=\"MODEL\",\n" +\
                         "/\n\n" +\
                         "!WU_TEMPLATE_VERSION=43r3-seasonal-20250801\n"+\
                         f"!EXPTID={experiment_id}\n"+\
                         f"!UNIQUE_MEMBER_ID={member_id}\n"+\
                         "!HORIZ_RESOLUTION=159\n" +\
                         "!VERT_RESOLUTION=91\n" +\
                         "!GRID_TYPE=l_2\n" +\
                         f"!UPLOAD_INTERVAL={upload_interval}\n" +\
                         "\n"+\
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
    jf_namelist_path = create_project_zip(fort4_path, project_dir, arcname="fort.4")
    os.remove(fort4_path)   # remove unzipped version
    print(f"[setup] Wrote fort.4 and {jf_namelist_path.name}")

    # The OpenIFS BOINC implementation uses mapped logical filenames to
    # identify the various input files.  Here we create the files with
    # the logical names expected by the model.
    # It is less than ideal as it makes the code more complex to handle
    # resolving these files. It also makes it harder to debug as the
    # filenames are not descriptive.

    # Create logical namelist file
    namelist_path = slot0_dir / f"test_model_{member_id}_yyyymmddhh_{forecast_length}_{batch_id}_{wu_name}.zip"
    write_soft_link(namelist_path, Path(os.path.relpath(jf_namelist_path, slot0_dir)))
    print(f"[setup] Wrote logical namelist {namelist_path.name}")

    # Create ic_ancil file
    ic_ancil_payload = slot0_dir / "jf_ic_ancil"
    with open(ic_ancil_payload, 'a') as jf_ic_ancil_file:
      jf_ic_ancil_file.write(secrets.token_hex(4000) + '\n')
    jf_ic_ancil_path = create_project_zip(ic_ancil_payload, project_dir, arcname='jf_ic_ancil')
    os.remove(ic_ancil_payload)
    write_soft_link(slot0_dir / f"ic_ancil_{wu_name}.zip", Path(os.path.relpath(jf_ic_ancil_path, slot0_dir)))
    print(f"[setup] Wrote logical ic_ancil_{wu_name}.zip")

    # Create ifsdata file
    ifsdata_payload = slot0_dir / 'jf_ifsdata'
    with open(ifsdata_payload, 'a') as jf_ifsdata_file:
      jf_ifsdata_file.write(secrets.token_hex(4000) + '\n')
    jf_ifsdata_path = create_project_zip(ifsdata_payload, project_dir, arcname='jf_ifsdata')
    os.remove(ifsdata_payload)
    write_soft_link(slot0_dir / f"ifsdata_{wu_name}.zip", Path(os.path.relpath(jf_ifsdata_path, slot0_dir)))
    print(f"[setup] Wrote logical ifsdata_{wu_name}.zip")

    # Create clim_data file
    clim_data_payload = slot0_dir / 'jf_clim_data'
    with open(clim_data_payload, 'a') as jf_clim_data_file:
      jf_clim_data_file.write(secrets.token_hex(4000) + '\n')
    jf_clim_data_path = create_project_zip(clim_data_payload, project_dir, arcname='jf_clim_data')
    os.remove(clim_data_payload)
    write_soft_link(slot0_dir / f"clim_data_{wu_name}.zip", Path(os.path.relpath(jf_clim_data_path, slot0_dir)))
    print(f"[setup] Wrote logical clim_data_{wu_name}.zip")

    print("[setup] Test fixture generation complete")
