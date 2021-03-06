//////////////////////////////////////////////////////////////////////////
// G4VoxelData
// ===========
// A general interface for loading voxelised data as geometry in GEANT4.
//
// Author:  Christopher M Poole <mail@christopherpoole.net>
// Source:  http://github.com/christopherpoole/G4VoxelData
//
// License & Copyright
// ===================
// 
// Copyright 2013 Christopher M Poole <mail@christopherpoole.net>
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////////

// G4VOXELDATA //
#include "G4VoxelData.hh"
#include "G4VoxelArray.hh"

#ifndef G4VOXELDATAPARAMETERISATION_HH
#define G4VOXELDATAPARAMETERISATION_HH

// STL //
#include <vector>
#include <map>

// GEANT4 //
#include "globals.hh"
#include "G4Types.hh"
#include "G4ThreeVector.hh"
#include "G4PVParameterised.hh"
#include "G4VNestedParameterisation.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4VPhysicalVolume.hh"
#include "G4VTouchable.hh"
#include "G4ThreeVector.hh"
#include "G4Box.hh"
#include "G4LogicalVolume.hh"
#include "G4Material.hh"
#include "G4VisAttributes.hh"


template <typename T, typename U=T>
class G4VoxelDataParameterisation : public G4VNestedParameterisation
{
public:
    G4VoxelDataParameterisation(){
    };

    G4VoxelDataParameterisation(G4VoxelArray<T>* array,
        std::map<U, G4Material*> materials_map, G4VPhysicalVolume* mother_physical)
    {
        this->array = array;

        this->shape = this->array->GetShape();
        this->spacing = this->array->GetSpacing();

        this->materials_map = materials_map;
        this->mother_physical = mother_physical;

        this->voxel_size = array->GetVoxelSize();
        this->volume_shape = array->GetVolumeShape();
   
        this->round_values = false; 
        this->trim_values = false; 

        this->visibility = false;
        this->show_bounding_box = false;

        // User defined nth planes in each direction where every (shape/2)+1
        // plane starting at 0 will show the mid planes, assuming no cropping,
        // every 1st plane starting at 0 will show everything (default).
        this->xth_plane = 1;
        this->xth_offset = 0;
        this->yth_plane = 1;
        this->yth_offset = 0;
        this->zth_plane = 1;
        this->zth_offset = 0;

        // Rounding
        this->rounder = 0;
        this->lower_bound = 0;
        this->upper_bound = 0;
    };

    virtual ~G4VoxelDataParameterisation(){
    };

    virtual void Construct(G4ThreeVector position, G4RotationMatrix* rotation) {
        G4NistManager* nist_manager = G4NistManager::Instance();
        G4Material* air = nist_manager->FindOrBuildMaterial("G4_AIR");

        G4Box* voxeldata_solid =
            new G4Box("voxeldata_solid", shape[0]*spacing[0]/2.,
                                         shape[1]*spacing[1]/2.,
                                         shape[2]*spacing[2]/2.);
        voxeldata_logical =
            new G4LogicalVolume(voxeldata_solid, air, "voxeldata_logical", 0, 0, 0);
        new G4PVPlacement(rotation, position,
            "voxeldata_container", voxeldata_logical, mother_physical, 0, false, 0);
        if (!this->show_bounding_box)
            voxeldata_logical->SetVisAttributes(G4VisAttributes::Invisible);

        // Y //
        G4VSolid* y_solid =
            new G4Box("y_solid", shape[0]*spacing[0]/2.,
                                 spacing[1]/2.,
                                 shape[2]*spacing[2]/2.);
        y_logical = new G4LogicalVolume(y_solid, air, "y_logical");
        new G4PVReplica("y_replica", y_logical, voxeldata_logical,
                        kYAxis, shape[1], spacing[1]);
//        if (!this->visibility)
            y_logical->SetVisAttributes(G4VisAttributes::Invisible);

        // X //
        G4VSolid* x_solid =
            new G4Box("x_solid", spacing[0]/2.,
                                 spacing[1]/2.,
                                 shape[2]*spacing[2]/2.);
        x_logical = new G4LogicalVolume(x_solid, air, "x_logical");
        new G4PVReplica("x_replica", x_logical, y_logical, kXAxis, shape[0],
                        spacing[0]);
//        if (!this->visibility)
            x_logical->SetVisAttributes(G4VisAttributes::Invisible);

        // VOXEL //
        G4VSolid* voxel_solid =
            new G4Box("voxel_solid", spacing[0]/2.,
                                     spacing[1]/2.,
                                     spacing[2]/2.);
        voxel_logical = new G4LogicalVolume(voxel_solid, air, "voxel_logical");
        if (!this->visibility)
            voxel_logical->SetVisAttributes(G4VisAttributes::Invisible);
        
        new G4PVParameterised("voxel_data", voxel_logical, x_logical, kUndefined, shape[2], this);
    };

    using G4VNestedParameterisation::ComputeMaterial;
    G4Material* ComputeMaterial(G4VPhysicalVolume *physical_volume,
            const G4int copy_number, const G4VTouchable *parent_touchable)
    {
        G4int x = parent_touchable->GetReplicaNumber(0) * array->GetMergeSize()[0];
        G4int y = parent_touchable->GetReplicaNumber(1) * array->GetMergeSize()[1];
        G4int z = copy_number * array->GetMergeSize()[2];

        if (z < 0) z = 0;

        // Correct index for cropping distances
        unsigned int offset_x = array->GetCropLimit()[0];
        unsigned int offset_y = array->GetCropLimit()[2];
        unsigned int offset_z = array->GetCropLimit()[4];
        
        G4Material* VoxelMaterial = GetMaterial(x + offset_x, y + offset_y, z + offset_z);
        physical_volume->GetLogicalVolume()->SetMaterial(VoxelMaterial);

        if (this->visibility) {
            G4Colour colour = *(GetColour(x + offset_x, y + offset_y, z + offset_z));

            if ((x + 1) % xth_plane == xth_offset ||
                (y + 1) % yth_plane == yth_offset ||
                (z + 1) % zth_plane == zth_offset)
            {
                physical_volume->GetLogicalVolume()->SetVisAttributes(colour);
            } else {
                physical_volume->GetLogicalVolume()->SetVisAttributes(
                        G4VisAttributes::Invisible);
            }
        }

        return VoxelMaterial;
    };

    G4int GetNumberOfMaterials() const
    {
        return array->GetLength();
    };

    G4Material* GetMaterial(G4int i) const
    {
        U value;
        
        if (round_values && trim_values) {
            value = array->GetRoundedValue(i, lower_bound, upper_bound, rounder);
        } else if (round_values && !trim_values) {
            value = array->GetRoundedValue(i, rounder); 
        } else {
            value = array->GetValue(i);
        }

        return materials_map.at(value);
    };

    G4Material* GetMaterial(unsigned int x, unsigned int y, unsigned int z)
    {
        if (array->IsMerged()) {
            std::vector<unsigned int> indices;
            indices.push_back(x);
            indices.push_back(y);
            indices.push_back(z);

            double val = 0;
            unsigned int merged_voxels = 1;
            for (unsigned int axis=0; axis<array->GetDimensions(); axis++) {
                unsigned int stride = array->GetMergeSize()[axis];
                merged_voxels *= stride;

                if (stride == 1) {
                    // Not actually merging voxels in this direction
                    continue;
                }

                for (unsigned int offset=0; offset<stride; offset++) {
                    indices[axis] += offset;
                    val += array->GetValue(array->GetIndex(indices));
                }
            }

            if (merged_voxels > 1) {   
                val = (U) val/merged_voxels;

                if (rounder && !(lower_bound && upper_bound)) {
                    val = array->RoundValue(val, rounder);
                } else if (rounder && lower_bound && upper_bound) {
                    val = array->RoundValue(val, lower_bound, upper_bound, rounder);
                }
                return materials_map.at(val);
            }
        }
        return GetMaterial(array->GetIndex(x, y, z));
    };

    unsigned int GetMaterialIndex( unsigned int copyNo) const
    {
        return copyNo;   
    };

    G4Colour* GetColour(G4int i) const
    {
        U value;
        if (round_values && trim_values) {
            value = array->GetRoundedValue(i, lower_bound, upper_bound, rounder);
        } else if (round_values && !trim_values) {
            value = array->GetRoundedValue(i, rounder); 
        } else {
            value = array->GetValue(i);
        }

        return colour_map.at(value);
    };

    G4Colour* GetColour(unsigned int x, unsigned int y, unsigned int z) {
        return GetColour(array->GetIndex(x, y, z));
    };

    void ComputeTransformation(const G4int copyNo, G4VPhysicalVolume *physVol) const
    {
        G4double x = 0;
        G4double y = 0;
        G4double z = 2*copyNo*voxel_size.z() - shape[2]*voxel_size.z() + voxel_size.z();

        G4ThreeVector position(x, y, z);
        physVol->SetTranslation(position);
    };

    using G4VNestedParameterisation::ComputeDimensions;
    void ComputeDimensions(G4Box& box, const G4int, const G4VPhysicalVolume *) const
    {
        box.SetXHalfLength(voxel_size.x());
        box.SetYHalfLength(voxel_size.y());
        box.SetZHalfLength(voxel_size.z());
    };

    G4LogicalVolume* GetLogicalVolume() {
        return voxel_logical;
    };

    void SetVisibility(G4bool visibility) {
        this->visibility = visibility;
    };

    void ShowXPlanes(unsigned int plane, unsigned int offset) {
        this->xth_plane = plane;
        this->xth_offset = offset;
    };
    void ShowYPlanes(unsigned int plane, unsigned int offset) {
        this->yth_plane = plane;
        this->yth_offset = offset;
    };

    void ShowZPlanes(unsigned int plane, unsigned int offset) {
        this->zth_plane = plane;
        this->zth_offset = offset;
    };

    void ShowPlanes(unsigned int xplane, unsigned int xoffset,
                       unsigned int yplane, unsigned int yoffset,
                       unsigned int zplane, unsigned int zoffset) {
        ShowXPlanes(xplane, xoffset);
        ShowYPlanes(yplane, yoffset);
        ShowZPlanes(zplane, zoffset);
    };

    void ShowMidPlanes() {
        ShowXPlanes(array->GetShape()[0]/2 + 1, 0);
        ShowYPlanes(array->GetShape()[1]/2 + 1, 0);
        ShowZPlanes(array->GetShape()[2]/2 + 1, 0);
    }

    void SetColourMap(std::map<U, G4Colour*> colour_map) {
        SetVisibility(true);
        this->colour_map = colour_map;
    };

    void SetRounding(T rounder) {
        round_values = true;

        this->rounder = rounder;
    }

    void SetRounding(T rounder, T lower_bound, T upper_bound) {
        round_values = true;
        trim_values = true;

        this->rounder = rounder;
        this->lower_bound = lower_bound;
        this->upper_bound = upper_bound;
    }

  private:
    G4ThreeVector voxel_size;
    G4ThreeVector volume_shape;

    std::vector<G4Material*> fMaterials;//array of pointers to materials
    size_t* fMaterialIndices; // Index in materials corresponding to each voxel

    std::map<U, G4Material*> materials_map;
    G4VoxelData* voxel_data;
    G4VoxelArray<T>* array;
    G4bool with_map;

    std::vector<unsigned int> shape;
    std::vector<double> spacing;

    G4VPhysicalVolume* mother_physical;

    G4LogicalVolume* voxeldata_logical;
    G4LogicalVolume* voxel_logical;
    G4LogicalVolume* x_logical;
    G4LogicalVolume* y_logical;

    G4bool visibility;
    G4bool show_bounding_box;
    G4bool show_user_planes;
    unsigned int xth_plane;
    unsigned int xth_offset;
    unsigned int yth_plane;
    unsigned int yth_offset;
    unsigned int zth_plane;
    unsigned int zth_offset;

    std::map<U, G4Colour*> colour_map;
  
    // For reading array as rounded to some increment 
    G4bool round_values;
    G4bool trim_values;
    T lower_bound;
    T upper_bound;
    T rounder;
};

#endif // G4VOXELDATAPARAMETERISATION_HH
