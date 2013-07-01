/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2013 UniPro <ugene@unipro.ru>
 * http://ugene.unipro.ru
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include <U2Core/FailTask.h>
#include <U2Core/U2OpStatusUtils.h>
#include <U2Core/U2SafePoints.h>
#include <U2Core/AppContext.h>

#include <U2Lang/ActorPrototypeRegistry.h>
#include <U2Lang/BaseActorCategories.h>
#include <U2Lang/BaseTypes.h>
#include <U2Lang/BaseSlots.h>
#include <U2Lang/BasePorts.h>
#include <U2Lang/WorkflowEnv.h>

#include <U2Designer/DelegateEditors.h>

#include "Snp2PdbSiteWorker.h"

namespace U2 {

namespace LocalWorkflow {

const QString Snp2PdbSiteWorkerFactory::ACTOR_ID( "snp2pdb-site" );

static const QString DB_PATH("db_path");

/************************************************************************/
/* Worker */
/************************************************************************/

Snp2PdbSiteWorker::Snp2PdbSiteWorker( Actor *p )
    : BaseRequestForSnpWorker( p )
{

}

QVariantMap Snp2PdbSiteWorker::getInputDataForRequest( const U2Variant& variant,
    const U2VariantTrack& track, U2Dbi* dataBase )
{
    QVariantMap inputData;
    // TODO: obtain pdb id and chain from db

    //sample data
    //inputData[SnpRequestKeys::SNP_2_PDB_SITE_PDB_ID] = "1GZH";
    //inputData[SnpRequestKeys::SNP_2_PDB_SITE_CHAIN] = "A";
    //inputData[SnpRequestKeys::SNP_2_PDB_SITE_MUTATIONS] = "A 110       R -> C";

    inputData[SnpRequestKeys::SNP_2_PDB_SITE_PDB_ID] = "";
    inputData[SnpRequestKeys::SNP_2_PDB_SITE_CHAIN] = "";
    // TODO: construct the parameter below using the following notation:
    // <`pdb_chain` `snp_position`       `source_aminoacid` -> `replacing_aminoacid`>
    inputData[SnpRequestKeys::SNP_2_PDB_SITE_MUTATIONS] = "";

    return inputData;
}

QString Snp2PdbSiteWorker::getRequestingScriptName( ) const
{
    return SnpRequestingScripts::SNP_2_PDB_SITE_SCRIPT;
}

QString Snp2PdbSiteWorker::getDatabasePath() const
{
    return getValue<QString>( DB_PATH );
}

QList<SnpResponseKey> Snp2PdbSiteWorker::getResultKeys( ) const
{
    QList<SnpResponseKey> result;
    // TODO: add other keys
    result << SnpResponseKeys::SNP_2_PDB_SITE_;
    return result;
}

/************************************************************************/
/* Factory */
/************************************************************************/

Snp2PdbSiteWorkerFactory::Snp2PdbSiteWorkerFactory( )
    : DomainFactory( ACTOR_ID )
{

}

void Snp2PdbSiteWorkerFactory::init( )
{
    QList<PortDescriptor*> p;
    QList<Attribute*> a;
    {
        Descriptor sd( BasePorts::IN_VARIATION_TRACK_PORT_ID( ), "Input variations",
            "Variations for annotations." );
        Descriptor od( BasePorts::OUT_VARIATION_TRACK_PORT_ID( ), "Output variations",
            "Variations with annotations." );

        QMap<Descriptor, DataTypePtr> modelM;
        QMap<Descriptor, DataTypePtr> inM;
        inM[BaseSlots::VARIATION_TRACK_SLOT( )] = BaseTypes::VARIATION_TRACK_TYPE( );
        p << new PortDescriptor( sd, DataTypePtr( new MapDataType( "in.variations", inM ) ),
            true /*input*/ );
        QMap<Descriptor, DataTypePtr> outM;
        p << new PortDescriptor( od, DataTypePtr( new MapDataType( "out.variations", outM ) ),
            false /*input*/, true /*multi*/ );
    }

    Descriptor dbPath( DB_PATH, QObject::tr( "Database path" ),
        QObject::tr( "Path to SNP database." ) );
    a << new Attribute( dbPath, BaseTypes::STRING_TYPE( ), true, "" );

    QMap<QString, PropertyDelegate *> delegates;
    {
        delegates[DB_PATH] = new URLDelegate( "", "", false );
    }

    Descriptor protoDesc( Snp2PdbSiteWorkerFactory::ACTOR_ID,
        QObject::tr( "SNP affect on PDB sites" ),
        QObject::tr( "Identification of the SNP influence on PDB sites" ) );

    ActorPrototype *proto = new IntegralBusActorPrototype( protoDesc, p, a );
    proto->setPrompter( new Snp2PdbSitePrompter( ) );
    proto->setEditor( new DelegateEditor( delegates ) );
    WorkflowEnv::getProtoRegistry( )->registerProto( BaseActorCategories::CATEGORY_SCHEMAS( ),
        proto );
    WorkflowEnv::getDomainRegistry( )->getById( LocalDomainFactory::ID )->registerEntry(
        new Snp2PdbSiteWorkerFactory( ) );
}

Worker *Snp2PdbSiteWorkerFactory::createWorker( Actor *a )
{
    return new Snp2PdbSiteWorker( a );
}

/************************************************************************/
/* Prompter */
/************************************************************************/

Snp2PdbSitePrompter::Snp2PdbSitePrompter( Actor *p )
    : PrompterBase<Snp2PdbSitePrompter>( p )
{

}

QString Snp2PdbSitePrompter::composeRichDoc( )
{
    QString res = ""; 
    Actor* annProducer = qobject_cast<IntegralBusPort*>(
        target->getPort(BasePorts::IN_VARIATION_TRACK_PORT_ID( ) ) )->getProducer(
        BaseSlots::VARIATION_TRACK_SLOT( ).getId( ) );

    QString unsetStr = "<font color='red'>" + tr("unset") + "</font>";
    QString annUrl = annProducer ? annProducer->getLabel( ) : unsetStr;
    QString path = getHyperlink(DB_PATH, getURL(DB_PATH));

    res.append(tr("Uses variations from <u>%1</u> as input.").arg(annUrl));
    res.append(tr(" Takes annotations from <u>%1</u> database.").arg(path.isEmpty() ?
        unsetStr : path));

    return res;
}

} // LocalWorkflow

} // U2
