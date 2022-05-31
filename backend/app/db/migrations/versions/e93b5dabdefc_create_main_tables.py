"""create_main_tables

Revision ID: e93b5dabdefc
Revises: 
Create Date: 2021-11-10 14:32:31.908806

"""

from typing import Tuple
from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic
revision = 'e93b5dabdefc'
down_revision = None
branch_labels = None
depends_on = None

def create_updated_at_trigger() -> None:
    op.execute(
        """
        CREATE OR REPLACE FUNCTION update_updated_at_column()
            RETURNS TRIGGER AS
        $$
        BEGIN
            NEW.updated_at = now();
            RETURN NEW;
        END;
        $$ language 'plpgsql';
        """
    )
    
    
def timestamps(indexed: bool = False) -> Tuple[sa.Column, sa.Column]:
    return (
        sa.Column(
            "created_at",
            sa.TIMESTAMP(timezone=True),
            server_default=sa.func.now(),
            nullable=False,
            index=indexed,
        ),
        sa.Column(
            "updated_at",
            sa.TIMESTAMP(timezone=True),
            server_default=sa.func.now(),
            nullable=False,
            index=indexed,
        ),
    )


def create_patients_table() -> None:
    op.create_table(
        "patients",
        sa.Column("id", sa.Integer, primary_key=True),
        sa.Column("name", sa.Text, nullable=False, index=True),
        sa.Column("surname", sa.Text, nullable=True),
        sa.Column("dob", sa.Date, nullable=False), # datetime.date() object
        sa.Column("ethnicity", sa.Text, sa.ForeignKey('ethnicities.code') ), #  NO ondelete="CASCADE" here! Erasing ethnicites must not be permitted as long as patient of this ethnicity exists!
        sa.Column("sex", sa.Text, nullable=False),
        sa.Column("owner", sa.Integer, sa.ForeignKey("users.id", ondelete="CASCADE")),          
        *timestamps()
    )
    op.execute(
        """
        CREATE TRIGGER update_patient_modtime
            BEFORE UPDATE
            ON patients
            FOR EACH ROW
        EXECUTE PROCEDURE update_updated_at_column();
        """
    )


def create_ethnicities_table() -> None:
    """
    ethnicities table: ethnicity code, ethnicity name, continent name
    """
    op.create_table(
        "ethnicities",
        sa.Column("code", sa.Text, nullable=False, primary_key=True),
        sa.Column("name", sa.Text, nullable=False),
        sa.Column("country", sa.Text, nullable=True)
    )
    op.execute(
    """
    INSERT INTO ethnicities 
    VALUES ('CAUC', 'Caucasian', ' ');
    
    INSERT INTO ethnicities (code, name, country)
    VALUES ('CAPEC', 'Cape Coloured', 'South Africa');
    """    
    )
    

def create_facescans_table() -> None:
    """
    facescans table: id (primary key), patient_id (foreign key), scan_timestamp, imaging_modality, mesh, texture
    """
    op.create_table(
        "facescans",
        sa.Column("id", sa.Integer, primary_key=True),
        sa.Column("patient_id", sa.Integer, sa.ForeignKey('patients.id', ondelete="CASCADE")),
        sa.Column("scan_date", sa.Date, nullable=False),
        sa.Column("thumbnail", sa.LargeBinary, nullable=True),
        sa.Column("imaging_modality", sa.Text, nullable=True, server_default=""),
        sa.Column("meshdatatype", sa.Text, nullable=True, server_default="OBJ"),
        sa.Column("mesh", sa.LargeBinary, nullable=False),
        sa.Column("texturedatatype", sa.Text, nullable=True, server_default="PNG"),
        sa.Column("texture", sa.LargeBinary, nullable=True),
        sa.Column("owner", sa.Integer, sa.ForeignKey("users.id", ondelete="CASCADE")),          
        *timestamps()
    )
    op.execute(
        """
        CREATE TRIGGER update_facescan_modtime
            BEFORE UPDATE
            ON facescans
            FOR EACH ROW
        EXECUTE PROCEDURE update_updated_at_column();
        """
    )


def create_landmarksets_table() -> None:
    op.create_table(
        "landmarksets",
        sa.Column("code", sa.Text, nullable=False, primary_key=True, server_default=""),
        sa.Column("landmarklist", sa.JSON, nullable=False),
        *timestamps()
    )
    op.execute(
        """
        INSERT INTO landmarksets
        SELECT code, landmarklist
        FROM json_populate_record (NULL::landmarksets,
            '{
              "code": "UNSPEC",
              "landmarklist": [""]
            }'
        );
        INSERT INTO landmarksets
        SELECT code, landmarklist
        FROM json_populate_record (NULL::landmarksets,
            '{
              "code": "3POINT_USER_SELECTION",
              "landmarklist": ["pupil_left","pupil_right", "mouth_centre"]
            }'
        );
        INSERT INTO landmarksets
        SELECT code, landmarklist
        FROM json_populate_record (NULL::landmarksets,
            '{
              "code": "CNN20points",
              "landmarklist": ["right_ex", "right_en", "left_en", "left_ex", "left_cupid", "lip_centre", "right_cupid", "right_ch", "left_ch", "nasion", "gnathion", "left_alare", "right_alare", "pronasale", "subnasale", "lower_right_ear_attachment", "lower_left_ear_attachment", "lower_lip_centre", "right_tragion", "left_tragion"]
            }'
        );
        """
    )


def create_landmarks_table() -> None:
    op.create_table(
        "landmarks",
        sa.Column("id", sa.Integer, primary_key=True),
        sa.Column("facescan_id", sa.Integer, sa.ForeignKey('facescans.id', ondelete="CASCADE")),
        sa.Column("landmarksetcode", sa.Text, sa.ForeignKey('landmarksets.code')),
        sa.Column("algorithm", sa.Text, nullable=True),
        sa.Column("computationstatus", sa.Text, nullable=True),        
        sa.Column("landmarks", sa.JSON, nullable=True),
        sa.Column("owner", sa.Integer, sa.ForeignKey("users.id", ondelete="CASCADE")),          
        *timestamps()
    )
    op.execute(
        """
        CREATE TRIGGER update_landmark_modtime
            BEFORE UPDATE
            ON landmarks
            FOR EACH ROW
        EXECUTE PROCEDURE update_updated_at_column();
        """
    )
    
def create_signatureModels_table() -> None:
    op.create_table(
        "signaturemodels",
        sa.Column("modelname", sa.Text, nullable=False, primary_key=True, server_default=""),
        sa.Column("modeldata", sa.LargeBinary, nullable=True)
    )
    op.execute(
        """
        INSERT INTO signaturemodels
        VALUES ('UNSPECIFIED', '');
        """
    )

    
def create_signatures_table() -> None:
    """
        Stores a heatmap with reference to landmarks used for registration. TODO: Consider storing a reference to the face model used to compute the heatmap.
    """
    op.create_table(
        "signatures",
        sa.Column("id", sa.Integer, primary_key=True),
        sa.Column("landmark_id", sa.Integer, sa.ForeignKey('landmarks.id', ondelete="CASCADE")),
        sa.Column("modelname", sa.Text, sa.ForeignKey("signaturemodels.modelname", ondelete="CASCADE")),   
        sa.Column("facialregion", sa.Text, sa.ForeignKey('facialregionnames.code', ondelete="CASCADE")),
        sa.Column("principal_components", sa.JSON, nullable=True),                               
        sa.Column("heatmap", sa.LargeBinary, nullable=False),       # should contain vtkPolyData with scalars
        sa.Column("owner", sa.Integer, sa.ForeignKey("users.id", ondelete="CASCADE")),          
        *timestamps()
    )
    op.execute(
        """
        CREATE TRIGGER update_signature_modtime
            BEFORE UPDATE
            ON signatures
            FOR EACH ROW
        EXECUTE PROCEDURE update_updated_at_column();
        """
    )
    
def create_facialRegionNames_table() -> None:
    op.create_table(
        "facialregionnames",
        sa.Column("code", sa.Text, nullable=False, primary_key=True, server_default=""),
        sa.Column("facialregionname", sa.Text, nullable=False)
    )
    op.execute(
        """
        INSERT INTO facialregionnames
        VALUES ('FACE', 'Face');

        INSERT INTO facialregionnames
        VALUES ('EYES', 'Eyes');
    
        INSERT INTO facialregionnames
        VALUES ('NOSE', 'Nose');

        INSERT INTO facialregionnames
        VALUES ('MALAR', 'Malar');

        INSERT INTO facialregionnames
        VALUES ('PHILTRUM', 'Philtrum');

        INSERT INTO facialregionnames
        VALUES ('MOUTH', 'Mouth');

        INSERT INTO facialregionnames
        VALUES ('PROFILE', 'Profile');            
        """
    )


def create_meanClassificationModels_table() -> None:
    op.create_table(
        "meanclassificationmodels",
        sa.Column("modelname", sa.Text, nullable=False, primary_key=True, server_default=""),
        sa.Column("modeldata", sa.LargeBinary, nullable=True)
    )
    op.execute(
        """
        INSERT INTO meanclassificationmodels
        VALUES ('UNSPECIFIED', '');
        """
    )
    

def create_meanClassificationResults_table() -> None:
    op.create_table(
        "meanclassificationresults",
        sa.Column("id", sa.Integer, primary_key=True),
        sa.Column("landmark_id", sa.Integer, sa.ForeignKey('landmarks.id', ondelete="CASCADE")),        
        sa.Column("modelname", sa.Text, sa.ForeignKey("meanclassificationmodels.modelname", ondelete="CASCADE")),           
        sa.Column("facialregion", sa.Text, sa.ForeignKey('facialregionnames.code', ondelete="CASCADE")),               
        sa.Column("meanvalue", sa.Float, nullable=False),
        sa.Column("stderror", sa.Float, nullable=False),
        sa.Column("neg_class", sa.Text, nullable=True),
        sa.Column("pos_class", sa.Text, nullable=True),        
        sa.Column("owner", sa.Integer, sa.ForeignKey("users.id", ondelete="CASCADE")),                  
        *timestamps()
    )
    op.execute(
        """
        CREATE TRIGGER update_meanclassificationresults_modtime
            BEFORE UPDATE
            ON meanclassificationresults
            FOR EACH ROW
        EXECUTE PROCEDURE update_updated_at_column();
        """
    )


def create_patientnotes_table() -> None:
    op.create_table(
        "patientnotes",
        sa.Column("id", sa.Integer, primary_key=True),
        sa.Column("patient_id", sa.Integer, sa.ForeignKey('patients.id', ondelete="CASCADE")),
        sa.Column("note", sa.Text, nullable=True, server_default=""),
        sa.Column("owner", sa.Integer, sa.ForeignKey("users.id", ondelete="CASCADE")),          
        *timestamps()
    )
    op.execute(
        """
        CREATE TRIGGER update_patientnotes_modtime
            BEFORE UPDATE
            ON patientnotes
            FOR EACH ROW
        EXECUTE PROCEDURE update_updated_at_column();
        """
    )


def create_users_table() -> None:
    op.create_table(
        "users",
        sa.Column("id", sa.Integer, primary_key=True),
        sa.Column("username", sa.Text, unique=True, nullable=False, index=True),        
        sa.Column("email", sa.Text, unique=True, nullable=False, index=True),
        sa.Column("email_verified", sa.Boolean, nullable=False, server_default="False"),
        sa.Column("salt", sa.Text, nullable=False),
        sa.Column("password", sa.Text, nullable=False),
        sa.Column("is_active", sa.Boolean(), nullable=False, server_default="True"),
        sa.Column("is_superuser", sa.Boolean(), nullable=False, server_default="False"),     
        *timestamps()
    )
    op.execute(
        """
        CREATE TRIGGER update_user_modtime
            BEFORE UPDATE
            ON users
            FOR EACH ROW
        EXECUTE PROCEDURE update_updated_at_column();
        """
    )


def create_profiles_table() -> None:
    op.create_table(
        "profiles",
        sa.Column("id", sa.Integer, primary_key=True),
        sa.Column("full_name", sa.Text, nullable=True),
        sa.Column("phone_number", sa.Text, nullable=True),
        sa.Column("bio", sa.Text, nullable=True, server_default=""),
        sa.Column("image", sa.LargeBinary, nullable=True),
        sa.Column("user_id", sa.Integer, sa.ForeignKey("users.id", ondelete="CASCADE")),
        *timestamps(),
    )
    op.execute(
        """
        CREATE TRIGGER update_profiles_modtime
            BEFORE UPDATE
            ON profiles
            FOR EACH ROW
        EXECUTE PROCEDURE update_updated_at_column();
        """
    )


def upgrade() -> None:
    create_updated_at_trigger()
    create_users_table()
    create_profiles_table()
    
    create_ethnicities_table()
    create_patients_table()
    create_facescans_table()
    create_patientnotes_table()
    
    create_landmarksets_table()
    create_landmarks_table()

    create_facialRegionNames_table()
    create_meanClassificationModels_table()
    
    create_signatureModels_table()
    create_signatures_table()
        
    create_meanClassificationResults_table()


def downgrade() -> None: 
    
    op.drop_table("meanclassificationresults")

    op.drop_table("signatures")   
    op.drop_table("signaturemodels")    


    op.drop_table("meanclassificationmodels")
    op.drop_table("facialregionnames")
    
    op.drop_table("landmarks")
    op.drop_table("landmarksets")

    op.drop_table("patientnotes")
    op.drop_table("facescans")
    op.drop_table("patients")
    op.drop_table("ethnicities")
    
    op.drop_table("profiles")
    op.drop_table("users")
    op.execute("DROP FUNCTION update_updated_at_column")
    

