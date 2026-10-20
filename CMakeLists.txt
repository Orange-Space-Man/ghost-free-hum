cmake_minimum_required(VERSION 3.24)
project(ghost-free-hum VERSION 0.2.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "The current platform implementation requires Windows and Direct3D 11.")
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(ResourceIO STATIC
    src/resources/dolresources.cpp
    src/resources/gameassetpackage.cpp
    src/resources/roomarchivereader.cpp
    src/resources/roomdiscreader.cpp
)

add_library(JLib STATIC
    src/jlib/j3d/j3danimationresource.cpp
    src/jlib/j3d/j3dmaterialfactory.cpp
    src/jlib/j3d/j3dmodelgeometry.cpp
    src/jlib/j3d/j3dmodelhierarchy.cpp
    src/jlib/j3d/j3dmodelinstance.cpp
    src/jlib/j3d/j3dnametable.cpp
    src/jlib/jaudio/jaudiodspmixer.cpp
    src/jlib/jaudio/jaudioeffectconfig.cpp
    src/jlib/jaudio/jaudioplayback.cpp
    src/jlib/jaudio/jaudioresources.cpp
    src/jlib/jaudio/jaudiosequence.cpp
    src/jlib/jaudio/jaudiosequencerandom.cpp
    src/jlib/jaudio/jaudiosequencecontrols.cpp
    src/jlib/jaudio/jaudioeffectcontrols.cpp
    src/jlib/jaudio/jaudioeffectvoice.cpp
    src/jlib/jaudio/jaudioeffectrelease.cpp
    src/jlib/jaudio/jaudioeffectadmission.cpp
    src/jlib/jaudio/jaudioeffectparameterpool.cpp
    src/jlib/jaudio/jaudioeffectselection.cpp
    src/jlib/jaudio/jaudiostream.cpp
    src/jlib/jaudio/jaudiovoice.cpp
)

add_library(Game STATIC
    src/game/actors/actoranimationsound.cpp
    src/game/actors/actoranimationsoundupdate.cpp
    src/game/actors/actorcapturedeformation.cpp
    src/game/actors/actorcapturemotion.cpp
    src/game/actors/actorcaptureownership.cpp
    src/game/actors/actorcaptureupdate.cpp
    src/game/actors/actorchildattachment.cpp
    src/game/actors/actorcollisionowner.cpp
    src/game/actors/actorcollisionquery.cpp
    src/game/actors/actorcollisionvolume.cpp
    src/game/actors/actorcontactevents.cpp
    src/game/actors/actorcontactframe.cpp
    src/game/actors/actoreffectstate.cpp
    src/game/actors/actorinitialpose.cpp
    src/game/actors/actorkeysampler.cpp
    src/game/actors/actormapcontact.cpp
    src/game/actors/actormodelanimation.cpp
    src/game/actors/actormodelarchive.cpp
    src/game/actors/actormodelcatalog.cpp
    src/game/actors/actormodelinstance.cpp
    src/game/actors/actormodelresources.cpp
    src/game/actors/actorpaircontact.cpp
    src/game/actors/actorpathsampler.cpp
    src/game/actors/actorposeblend.cpp
    src/game/actors/actorposeowner.cpp
    src/game/actors/actorposeupdate.cpp
    src/game/actors/actorregistry.cpp
    src/game/actors/actortimingsampler.cpp
    src/game/actors/actortrailcontroller.cpp
    src/game/actors/characteractorlinks.cpp
    src/game/actors/characterattachmentstate.cpp
    src/game/actors/charactercreationruntime.cpp
    src/game/actors/charactercreationstate.cpp
    src/game/actors/charactereffectresources.cpp
    src/game/actors/characterevent.cpp
    src/game/actors/charactergenerator.cpp
    src/game/actors/characteritemselection.cpp
    src/game/actors/characterowner.cpp
    src/game/actors/characterownerdamage.cpp
    src/game/actors/characterownerevents.cpp
    src/game/actors/characterownermotion.cpp
    src/game/actors/characterownerprojection.cpp
    src/game/actors/characterownership.cpp
    src/game/actors/characterownerupdates.cpp
    src/game/actors/characterownervacuum.cpp
    src/game/actors/characterplacementruntime.cpp
    src/game/actors/moveactor.cpp
    src/game/audio/gameplaymusicrouting.cpp
    src/game/audio/gameplaymusicowner.cpp
    src/game/audio/gameplayaudiocontext.cpp
    src/game/audio/jaudiopositionalplayback.cpp
    src/game/audio/jaudiopositionowners.cpp
    src/game/effects/barriereffectpool.cpp
    src/game/effects/bridgeeffectpool.cpp
    src/game/effects/cobwebeffectpool.cpp
    src/game/effects/crystaleffectpool.cpp
    src/game/effects/elementparticlepool.cpp
    src/game/effects/elementparticlescene.cpp
    src/game/effects/icejaileffectpool.cpp
    src/game/effects/icicleeffectpool.cpp
    src/game/effects/modeleffectmanager.cpp
    src/game/effects/modeleffectowner.cpp
    src/game/effects/modeleffectpool.cpp
    src/game/effects/particleadvancement.cpp
    src/game/effects/particlebatch.cpp
    src/game/effects/particlechildren.cpp
    src/game/effects/particleeffectfactory.cpp
    src/game/effects/particleemission.cpp
    src/game/effects/particleemitterstate.cpp
    src/game/effects/particleforces.cpp
    src/game/effects/particleglowpool.cpp
    src/game/effects/particlenodestate.cpp
    src/game/effects/particleparameters.cpp
    src/game/effects/particlescene.cpp
    src/game/effects/particlescenecontext.cpp
    src/game/effects/particlesimulation.cpp
    src/game/effects/particlesuction.cpp
    src/game/effects/rivereffectpool.cpp
    src/game/effects/sharedmodeleffect.cpp
    src/game/effects/waterfalleffectpool.cpp
    src/game/enemies/enemybarrierstrategy.cpp
    src/game/enemies/enemybatgeneratorstrategy.cpp
    src/game/enemies/enemybombstrategy.cpp
    src/game/enemies/enemyconditionstrategy.cpp
    src/game/enemies/enemycreationcatalog.cpp
    src/game/enemies/enemycreationrequests.cpp
    src/game/enemies/enemyelementjewelstrategy.cpp
    src/game/enemies/enemyfakedoorstrategy.cpp
    src/game/enemies/enemyfiregeneratorstrategy.cpp
    src/game/enemies/enemygeneratordoorstrategy.cpp
    src/game/enemies/enemygeneratormanager.cpp
    src/game/enemies/enemygoldratgeneratorstrategy.cpp
    src/game/enemies/enemyheartescapestrategy.cpp
    src/game/enemies/enemyheartstrategy.cpp
    src/game/enemies/enemyitemappearance.cpp
    src/game/enemies/enemyitemlifetime.cpp
    src/game/enemies/enemyitemphysics.cpp
    src/game/enemies/enemyitemstrategy.cpp
    src/game/enemies/enemyjewelstrategy.cpp
    src/game/enemies/enemykeystrategy.cpp
    src/game/enemies/enemymanager.cpp
    src/game/enemies/enemymanagerruntime.cpp
    src/game/enemies/enemynegativerandomstrategy.cpp
    src/game/enemies/enemynegativestrategy.cpp
    src/game/enemies/enemypathcondition.cpp
    src/game/enemies/enemypearlstrategy.cpp
    src/game/enemies/enemyreplacestrategy.cpp
    src/game/enemies/enemyroomobserverstrategy.cpp
    src/game/enemies/enemystrategy.cpp
    src/game/enemies/enemystrategycatalog.cpp
    src/game/enemies/enemystrategyfactory.cpp
    src/game/enemies/enemytypecatalog.cpp
    src/game/enemies/enemytypeparameters.cpp
    src/game/enemies/itemcreationroute.cpp
    src/game/enemies/itemghostloot.cpp
    src/game/enemies/itemphysicsresponse.cpp
    src/game/enemies/itemspawnadmission.cpp
    src/game/enemies/itemsuctioncontroller.cpp
    src/game/enemies/itemsuctionforce.cpp
    src/game/events/eventconditions.cpp
    src/game/events/eventinfodecoder.cpp
    src/game/events/jmpentryactivation.cpp
    src/game/events/jmpeventstate.cpp
    src/game/events/jmpmessagesender.cpp
    src/game/events/jmptooldata.cpp
    src/game/events/missioneventactivation.cpp
    src/game/events/missionresources.cpp
    src/game/events/runtimeflags.cpp
    src/game/input/controllerfeedbackpulse.cpp
    src/game/input/controllerfeedbackscript.cpp
    src/game/input/controllerfeedbackstate.cpp
    src/game/math/originalfloattrig.cpp
    src/game/menu/fileselectcard.cpp
    src/game/menu/fileselectdetails.cpp
    src/game/menu/fileselectpanes.cpp
    src/game/menu/fileselectstate.cpp
    src/game/menu/gamefont.cpp
    src/game/menu/menulayout.cpp
    src/game/menu/menulayoutgraphics.cpp
    src/game/models/modeldrawmatrices.cpp
    src/game/models/modeldrawresource.cpp
    src/game/models/modeldrawtraversal.cpp
    src/game/models/modellighting.cpp
    src/game/models/modelmaterialstate.cpp
    src/game/models/modelresourceloader.cpp
    src/game/models/modeltextureimages.cpp
    src/game/models/modeltexturematrix.cpp
    src/game/models/modeltexturestages.cpp
    src/game/models/modelvertexstream.cpp
    src/game/models/s3danimation.cpp
    src/game/models/s3denvironment.cpp
    src/game/models/s3dfog.cpp
    src/game/models/s3dmodel.cpp
    src/game/models/s3dtexturestage.cpp
    src/game/models/s3dmorph.cpp
    src/game/models/s3dshadow.cpp
    src/game/player/playeractioncontext.cpp
    src/game/player/playeractionframe.cpp
    src/game/player/playeractionreaction.cpp
    src/game/player/playeractionrequests.cpp
    src/game/player/playeractionstate.cpp
    src/game/player/playeractionupdate.cpp
    src/game/player/playeractor.cpp
    src/game/player/playeractorcomponents.cpp
    src/game/player/playeractorview.cpp
    src/game/player/playerattachedadvance.cpp
    src/game/player/playerattachedframe.cpp
    src/game/player/playerattachedstate.cpp
    src/game/player/playerattachedupdate.cpp
    src/game/player/playerattractionevent.cpp
    src/game/player/playerauxiliaryowner.cpp
    src/game/player/playerbombdamage.cpp
    src/game/player/playercaptureflags.cpp
    src/game/player/playercomponentstate.cpp
    src/game/player/playercontactpool.cpp
    src/game/player/playercontactreaction.cpp
    src/game/player/playercontrolframe.cpp
    src/game/player/playercontrollerstate.cpp
    src/game/player/playercontrolstate.cpp
    src/game/player/playerenvironmentframe.cpp
    src/game/player/playerequipmentframe.cpp
    src/game/player/playereventdispatch.cpp
    src/game/player/playermoneycollection.cpp
    src/game/player/playereventpayload.cpp
    src/game/player/playergridmodels.cpp
    src/game/player/playericeeffectpool.cpp
    src/game/player/playerinputdecoder.cpp
    src/game/player/playerinputrouting.cpp
    src/game/player/playeritemcollection.cpp
    src/game/player/playerkeycollection.cpp
    src/game/player/playerliferecovery.cpp
    src/game/player/playerlightmodel.cpp
    src/game/player/playermanager.cpp
    src/game/player/playermanagerbinding.cpp
    src/game/player/playerorientation.cpp
    src/game/player/playerorientationframe.cpp
    src/game/player/playerorientationupdate.cpp
    src/game/player/playerpadsampler.cpp
    src/game/player/playerpipemodel.cpp
    src/game/player/playerreactionframe.cpp
    src/game/player/playerrecordwriteback.cpp
    src/game/player/playerresourcepools.cpp
    src/game/player/playerscannermodel.cpp
    src/game/player/playersharedmodels.cpp
    src/game/player/playershrinkevent.cpp
    src/game/player/playersuctioncompletion.cpp
    src/game/player/playertimerframe.cpp
    src/game/player/playervacuumcomponent.cpp
    src/game/player/playervacuumeffects.cpp
    src/game/player/playervacuummodel.cpp
    src/game/player/playervacuumqueries.cpp
    src/game/player/playervacuumquery.cpp
    src/game/player/playervelocitycontext.cpp
    src/game/player/playervelocityframe.cpp
    src/game/player/playerdoorapproach.cpp
    src/game/player/playerdoorinteraction.cpp
    src/game/player/playerdamageframe.cpp
    src/game/player/playerdoorframe.cpp
    src/game/player/playervelocityupdate.cpp
    src/game/player/playerweaponowner.cpp
    src/game/player/playerweaponparameters.cpp
    src/game/save/gamecardstorage.cpp
    src/game/save/gamesavestate.cpp
    src/game/scene/gamearchive.cpp
    src/game/scene/gamearchiveresources.cpp
    src/game/scene/gameentrystate.cpp
    src/game/scene/gameparameters.cpp
    src/game/scene/gameplaycamera.cpp
    src/game/scene/gameplaycameraowner.cpp
    src/game/scene/gameplaymanagerservices.cpp
    src/game/scene/gameplayresourceowner.cpp
    src/game/scene/gameplayspawn.cpp
    src/game/startup/keypresentationstate.cpp
    src/game/startup/openingresources.cpp
    src/game/startup/openingscenestate.cpp
    src/game/startup/openingtimeline.cpp
    src/game/startup/scenetransition.cpp
    src/game/startup/startupresources.cpp
    src/game/startup/startupscene.cpp
    src/game/world/cobwebplacementmanager.cpp
    src/game/world/dooranimationpose.cpp
    src/game/world/dooranimationresource.cpp
    src/game/world/dooranimationsampler.cpp
    src/game/world/doordescriptors.cpp
    src/game/world/doormodelhierarchy.cpp
    src/game/world/doorplayerevents.cpp
    src/game/world/doorpresentation.cpp
    src/game/world/doorresources.cpp
    src/game/world/doorstate.cpp
    src/game/world/mapcollisionprojection.cpp
    src/game/world/mapcollisionstate.cpp
    src/game/world/roomdependencies.cpp
    src/game/world/roomdoorplacement.cpp
    src/game/world/roomfurnitureowner.cpp
    src/game/world/roomfurnitureresources.cpp
    src/game/world/roomgeometry.cpp
    src/game/world/roomlightpool.cpp
    src/game/world/roomlocation.cpp
    src/game/world/roommaterials.cpp
    src/game/world/roommodelpose.cpp
    src/game/world/roommodelreader.cpp
    src/game/world/roomobjectplacement.cpp
    src/game/world/roomportalbounds.cpp
    src/game/world/roomresidency.cpp
    src/game/world/roomscenereader.cpp
    src/game/world/roomtraversalstate.cpp
    src/game/world/roomvisibilitywalk.cpp
    src/game/world/rotatingsurfacevelocity.cpp
)

add_library(WindowsRuntime STATIC
    src/game/menu/fileselectscene.cpp
    src/game/scene/gameplayscene.cpp
    src/game/startup/openingscene.cpp
    src/game/startup/keypresentationowner.cpp
    src/platform/d3d11/gamerenderer.cpp
    src/platform/d3d11/gameshadowrenderer.cpp
    src/platform/d3d11/modelbatchrenderer.cpp
    src/platform/d3d11/modelgeometryrenderer.cpp
    src/platform/d3d11/modelmaterialrenderer.cpp
    src/platform/d3d11/modelrenderstate.cpp
    src/platform/d3d11/modeltexturerenderer.cpp
    src/platform/windows/gameapplication.cpp
    src/platform/windows/gameassetsetup.cpp
    src/platform/windows/gameframepacing.cpp
    src/platform/windows/startupaudio.cpp
    src/platform/windows/windowpadinput.cpp
)

target_link_libraries(JLib PUBLIC ResourceIO)
add_library(ImGui STATIC
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_dx11.cpp
    third_party/imgui/backends/imgui_impl_win32.cpp
)
target_include_directories(ImGui PUBLIC third_party/imgui)
target_compile_features(ImGui PUBLIC cxx_std_17)
target_compile_definitions(ImGui PRIVATE UNICODE _UNICODE NOMINMAX)
target_sources(WindowsRuntime PRIVATE src/platform/windows/debugoverlay.cpp)
target_compile_definitions(WindowsRuntime PRIVATE GHOST_FREE_HUM_VERSION="${PROJECT_VERSION}" GHOST_FREE_HUM_BUILD="$<CONFIG>")
target_link_libraries(WindowsRuntime PUBLIC ImGui dwmapi)
target_link_libraries(Game PUBLIC JLib)
target_link_libraries(WindowsRuntime PUBLIC Game comdlg32 user32 gdi32 d3d11 dxgi d3dcompiler shell32 xaudio2 ole32 xinput9_1_0)

foreach(target ResourceIO JLib Game WindowsRuntime)
    target_include_directories(${target} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(${target} PUBLIC cxx_std_17)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    target_compile_definitions(${target} PUBLIC UNICODE _UNICODE NOMINMAX)
    if(MSVC)
        target_compile_options(${target} PUBLIC /EHsc /fp:precise)
        target_compile_options(${target} PRIVATE /W4 /permissive- /MP4)
    endif()
endforeach()

add_executable(ghost-free-hum WIN32 src/platform/windows/ghostfreehum.cpp)
target_link_libraries(ghost-free-hum PRIVATE WindowsRuntime)
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ghost-free-hum)
install(TARGETS ghost-free-hum RUNTIME DESTINATION .)
