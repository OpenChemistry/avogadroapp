"""Requests made the instant RPC answers must not race application startup."""

# The smallest CIF that Open Babel reads: one atom in a P 1 cell.
CIF = """data_first_request
_cell_length_a 5
_cell_length_b 5
_cell_length_c 5
_cell_angle_alpha 90
_cell_angle_beta 90
_cell_angle_gamma 90
_symmetry_space_group_name_H-M 'P 1'
loop_
_atom_site_label
_atom_site_fract_x
_atom_site_fract_y
_atom_site_fract_z
C1 0 0 0
"""


def test_open_cif_as_first_request(launch, tmp_path):
    """Open Babel's formats register in the background after the window
    answers RPC; file requests are held until they have (app #892), so a CIF
    opened as the very first request succeeds rather than failing with "No file
    format available"."""
    path = tmp_path / "first.cif"
    path.write_text(CIF)
    app = launch()  # a fresh app: nothing has warmed up the readers
    app.call("openFile", {"fileName": str(path)})
    assert app.info()["atomCount"] >= 1
