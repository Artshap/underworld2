/*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*
**                                                                                  **
** This file forms part of the Underworld geophysics modelling application.         **
**                                                                                  **
** For full license and copyright information, please refer to the LICENSE.md file  **
** located at the project root, or contact the authors.                             **
**                                                                                  **
**~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/
#include <sstream> 

#include <mpi.h>
#include <petsc.h>
extern "C" {
#include <StGermain/StGermain.h>
#include <StgDomain/StgDomain.h>
#include <StgFEM/StgFEM.h>
#include <PICellerator/PICellerator.h>
}

#include "FunctionIO.hpp"
#include "FEMCoordinate.hpp"
#include "MeshCoordinate.hpp"
#include "FeVariableFn.hpp"


Fn::FeVariableFn::FeVariableFn( void* fevariable ):Function(), _fevariable(fevariable){
    if(!Stg_Class_IsInstance( _fevariable, FeVariable_Type ))
        throw std::invalid_argument("Provided 'fevariable' does not appear to be of 'FeVariable' type.");


}

Fn::FeVariableFn::func Fn::FeVariableFn::getFunction( IOsptr sample_input )
{

    // setup output
    FeVariable* fevar = (FeVariable*)_fevariable;
    int numComponents = fevar->fieldComponentCount;

    std::shared_ptr<IO_double> _output = std::make_shared<IO_double>(numComponents, FunctionIO::Array);
    
    // if input is FEMCoordinate, eject appropriate lambda
    std::shared_ptr<const FEMCoordinate> femCoord = std::dynamic_pointer_cast<const FEMCoordinate>(sample_input);
    if ( femCoord ){
        if( femCoord->mesh() == (void*) (fevar->feMesh->parentMesh ) )
            return [_output,fevar](IOsptr input)->IOsptr {
                std::shared_ptr<const FEMCoordinate> femCoord = debug_dynamic_cast<const FEMCoordinate>(input);
                
                FeVariable_InterpolateWithinElement( fevar, femCoord->index(), femCoord->localCoord()->data(), _output->data() );

                return debug_dynamic_cast<const FunctionIO>(_output);
            };
    };

    // if input is MeshCoordinate, eject appropriate lambda
    std::shared_ptr<const MeshCoordinate> meshCoord = std::dynamic_pointer_cast<const MeshCoordinate>(sample_input);
    if ( meshCoord ){
        if( meshCoord->object() == (void*) (fevar->feMesh) )  // in this case, we need the identical mesh
            return [_output,fevar](IOsptr input)->IOsptr {
                std::shared_ptr<const MeshCoordinate> meshCoord = debug_dynamic_cast<const MeshCoordinate>(input);
                
                FeVariable_GetValueAtNode( fevar, meshCoord->index(), _output->data() );

                return debug_dynamic_cast<const FunctionIO>(_output);
            };
    }
    
    // if neither of the above worked, try plain old global coord
    std::shared_ptr<const IO_double> iodouble = std::dynamic_pointer_cast<const IO_double>(sample_input);
    if ( iodouble ){
        return [_output,fevar](IOsptr input)->IOsptr {
            std::shared_ptr<const IO_double> iodouble = debug_dynamic_cast<const IO_double>(input);            

            InterpolationResult retval = _FeVariable_InterpolateValueAt( fevar, iodouble->data(), _output->data() );

            if (! ( (retval == LOCAL) || (retval == SHADOW) ) ){
                std::stringstream streamguy;
                streamguy << "FeVariable interpolation at location (" << iodouble->at(0);
                for (unsigned ii=1; ii<iodouble->size(); ii++)
                    streamguy << ", "<< iodouble->at(ii);
                streamguy << ") does not appear to be valid.\nLocation is probably outside local domain.";
                
                throw std::runtime_error(streamguy.str());
            }

            return debug_dynamic_cast<const FunctionIO>(_output);
        };
    }
    
    // if we get here, something aint right
    throw std::invalid_argument("'FeVariableFn' does not appear to be compatible with provided input type.");

    
}

