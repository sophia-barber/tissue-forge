/*******************************************************************************
 * This file is part of Tissue Forge.
 * Copyright (c) 2022 T.J. Sego and Tien Comlekoglu
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 ******************************************************************************/

#include "tfSurfaceAreaConstraint.h"

#include <models/vertex/solver/tfVertex.h>
#include <models/vertex/solver/tfSurface.h>
#include <models/vertex/solver/tfBody.h>


using namespace TissueForge::models::vertex;


HRESULT SurfaceAreaConstraint::energy(MeshObj *source, MeshObj *target, float &e) {
    Body *b = (Body*)source;
    float darea = b->getArea() - constr;
    e = lam * darea * darea;
    return S_OK;
}

HRESULT SurfaceAreaConstraint::force(MeshObj *source, MeshObj *target, float *f) {
    Body *b = (Body*)source;
    Vertex *v = (Vertex*)target;
    
    FVector3 posc = v->getPosition();
    FVector3 ftotal(0.f);

    for(auto &s : v->getSurfaces()) {
        if(!s->in(b)) 
            continue;
        
        auto svertices = s->getVertices();
        unsigned int idx, idxc, idxp, idxn;
        FVector3 sftotal(0.f);
        
        for(idx = 0; idx < svertices.size(); idx++) {
            if(svertices[idx] == v) 
                idxc = idx;
            sftotal += Magnum::Math::cross(s->triangleNormal(idx).normalized(), 
                                           svertices[idx == svertices.size() - 1 ? 0 : idx + 1]->getPosition() - svertices[idx]->getPosition());
        }
        sftotal /= svertices.size();

        idxp = idxc == 0 ? svertices.size() - 1 : idxc - 1;
        idxn = idxc == svertices.size() - 1 ? 0 : idxc + 1;

        const FVector3 scentroid = s->getCentroid();

        sftotal += Magnum::Math::cross(s->triangleNormal(idxc).normalized(), scentroid - svertices[idxn]->getPosition());
        sftotal -= Magnum::Math::cross(s->triangleNormal(idxp).normalized(), scentroid - svertices[idxp]->getPosition());
        ftotal += sftotal;
    }

    ftotal *= (lam * (constr - b->getArea()));

    f[0] += ftotal[0];
    f[1] += ftotal[1];
    f[2] += ftotal[2];

    return S_OK;
}
