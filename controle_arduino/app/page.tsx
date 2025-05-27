"use client"

import { useState, useEffect } from "react"
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card"
import { Button } from "@/components/ui/button"
import { Input } from "@/components/ui/input"
import { Label } from "@/components/ui/label"
import { Badge } from "@/components/ui/badge"
import { Alert, AlertDescription } from "@/components/ui/alert"
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from "@/components/ui/table"
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogFooter,
  DialogHeader,
  DialogTitle,
  DialogTrigger,
} from "@/components/ui/dialog"
import {
  Trash2,
  Plus,
  Scan,
  Wifi,
  WifiOff,
  Fingerprint,
  CreditCard,
  CheckCircle,
  XCircle,
  Clock,
  AlertTriangle,
  Globe,
  Shield,
  ExternalLink,
} from "lucide-react"
import { useToast } from "@/hooks/use-toast"

interface FingerprintData {
  id: number
  status: string
}

interface AttendanceNotification {
  timestamp: string
  rfidTag: string
  fingerprintID?: number
  status: string
  message: string
  serverResponse?: string
}

export default function ESP8266AttendanceSystem() {
  const [esp8266URL, setEsp8266URL] = useState("") // Changed from IP to URL
  const [isConnected, setIsConnected] = useState(false)
  const [fingerprints, setFingerprints] = useState<FingerprintData[]>([])
  const [isLoading, setIsLoading] = useState(false)
  const [attendanceMode, setAttendanceMode] = useState(false)
  const [newFingerprintId, setNewFingerprintId] = useState("")
  const [isEnrollDialogOpen, setIsEnrollDialogOpen] = useState(false)
  const [currentNotification, setCurrentNotification] = useState<AttendanceNotification | null>(null)
  const [recentNotifications, setRecentNotifications] = useState<AttendanceNotification[]>([])
  const [showNotification, setShowNotification] = useState(false)
  const [isPublicTunnel, setIsPublicTunnel] = useState(false)
  const [ngrokInfo, setNgrokInfo] = useState<any>(null)
  const { toast } = useToast()

  // Detect if using tunnel service
  useEffect(() => {
    const isTunnel =
      esp8266URL.includes("ngrok") ||
      esp8266URL.includes("cloudflare") ||
      esp8266URL.includes("tunnel") ||
      esp8266URL.includes("https://")||esp8266URL.includes('lhr.life') 
    setIsPublicTunnel(isTunnel)
  }, [esp8266URL])

  // Get Ngrok tunnel info
  const getNgrokInfo = async () => {
    try {
      const response = await fetch("http://localhost:4040/api/tunnels")
      if (response.ok) {
        const data = await response.json()
        if (data.tunnels && data.tunnels.length > 0) {
          const tunnel = data.tunnels.find((t: any) => t.proto === "https") || data.tunnels[0]
          setNgrokInfo(tunnel)
          if (!esp8266URL) {
            setEsp8266URL(tunnel.public_url)
          }
        }
      }
    } catch (error) {
      console.log("Ngrok not running locally")
    }
  }

  useEffect(() => {
    getNgrokInfo()
    const interval = setInterval(getNgrokInfo, 10000) // Check every 10 seconds
    return () => clearInterval(interval)
  }, [])

  // Poll for live notifications when in attendance mode
  useEffect(() => {
    if (!attendanceMode || !isConnected) return

    const pollNotifications = async () => {
      try {
        const response = await fetch(`${esp8266URL}/live-notifications`)
        if (response.ok) {
          const data = await response.json()

          if (data.hasNewNotification && data.notification) {
            setCurrentNotification(data.notification)
            setShowNotification(true)

            // Auto-hide notification after 5 seconds
            setTimeout(() => {
              setShowNotification(false)
              clearNotification()
            }, 5000)
          }

          if (data.recentNotifications) {
            setRecentNotifications(data.recentNotifications)
          }
        }
      } catch (error) {
        console.error("Failed to poll notifications:", error)
      }
    }

    const interval = setInterval(pollNotifications, 500)
    return () => clearInterval(interval)
  }, [attendanceMode, isConnected, esp8266URL])

  // Clear notification on ESP8266
  const clearNotification = async () => {
    try {
      await fetch(`${esp8266URL}/clear-notification`, {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
      })
    } catch (error) {
      console.error("Failed to clear notification:", error)
    }
  }

  // Test connection to ESP8266
  const testConnection = async () => {
    if (!esp8266URL) {
      toast({
        title: "URL Required",
        description: "Please enter your ESP8266 URL or tunnel address",
        variant: "destructive",
      })
      return
    }

    setIsLoading(true)
    try {
      const response = await fetch(`${esp8266URL}/status`, {
        method: "GET",
        headers: {
          "Content-Type": "application/json",
        },
      })

      if (response.ok) {
        setIsConnected(true)
        toast({
          title: "Connected",
          description: `Successfully connected to ESP8266 via ${isPublicTunnel ? "tunnel" : "local network"}`,
        })
        await loadFingerprints()
      } else {
        throw new Error("Connection failed")
      }
    } catch (error) {
      setIsConnected(false)
      toast({
        title: "Connection Failed",
        description: "Could not connect to ESP8266. Check URL and tunnel status.",
        variant: "destructive",
      })
    } finally {
      setIsLoading(false)
    }
  }

  // Load all stored fingerprints
  const loadFingerprints = async () => {
    if (!isConnected) return

    setIsLoading(true)
    try {
      const response = await fetch(`${esp8266URL}/fingerprints`, {
        method: "GET",
        headers: {
          "Content-Type": "application/json",
        },
      })

      if (response.ok) {
        const data = await response.json()
        setFingerprints(data.fingerprints || [])
        toast({
          title: "Fingerprints Loaded",
          description: `Found ${data.fingerprints?.length || 0} stored fingerprints`,
        })
      } else {
        throw new Error("Failed to load fingerprints")
      }
    } catch (error) {
      toast({
        title: "Error",
        description: "Failed to load fingerprints from ESP8266",
        variant: "destructive",
      })
    } finally {
      setIsLoading(false)
    }
  }

  // Enroll new fingerprint
  const enrollFingerprint = async () => {
    if (!newFingerprintId || !isConnected) return

    const id = Number.parseInt(newFingerprintId)
    if (id < 1 || id > 127) {
      toast({
        title: "Invalid ID",
        description: "Fingerprint ID must be between 1 and 127",
        variant: "destructive",
      })
      return
    }

    setIsLoading(true)
    try {
      const response = await fetch(`${esp8266URL}/enroll`, {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify({ id: id }),
      })

      if (response.ok) {
        const data = await response.json()
        toast({
          title: "Enrollment Started",
          description: `Please place finger on sensor for ID ${id}. Follow ESP8266 serial monitor for instructions.`,
        })
        setIsEnrollDialogOpen(false)
        setNewFingerprintId("")
        setTimeout(() => loadFingerprints(), 10000)
      } else {
        const errorData = await response.json()
        throw new Error(errorData.message || "Enrollment failed")
      }
    } catch (error) {
      toast({
        title: "Enrollment Failed",
        description: error instanceof Error ? error.message : "Failed to start fingerprint enrollment",
        variant: "destructive",
      })
    } finally {
      setIsLoading(false)
    }
  }

  // Delete fingerprint
  const deleteFingerprint = async (id: number) => {
    if (!isConnected) return

    setIsLoading(true)
    try {
      const response = await fetch(`${esp8266URL}/delete`, {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify({ id: id }),
      })

      if (response.ok) {
        toast({
          title: "Fingerprint Deleted",
          description: `Successfully deleted fingerprint ID ${id}`,
        })
        await loadFingerprints()
      } else {
        const errorData = await response.json()
        throw new Error(errorData.message || "Deletion failed")
      }
    } catch (error) {
      toast({
        title: "Deletion Failed",
        description: error instanceof Error ? error.message : "Failed to delete fingerprint",
        variant: "destructive",
      })
    } finally {
      setIsLoading(false)
    }
  }

  // Toggle attendance mode
  const toggleAttendanceMode = async () => {
    if (!isConnected) return

    setIsLoading(true)
    try {
      const response = await fetch(`${esp8266URL}/attendance`, {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify({ mode: !attendanceMode }),
      })

      if (response.ok) {
        setAttendanceMode(!attendanceMode)
        setShowNotification(false)
        setCurrentNotification(null)
        toast({
          title: attendanceMode ? "Attendance Mode Stopped" : "Attendance Mode Started",
          description: attendanceMode ? "System returned to setup mode" : "Ready to scan RFID cards and fingerprints",
        })
      } else {
        throw new Error("Failed to toggle attendance mode")
      }
    } catch (error) {
      toast({
        title: "Error",
        description: "Failed to toggle attendance mode",
        variant: "destructive",
      })
    } finally {
      setIsLoading(false)
    }
  }

  // Get status icon and color based on notification status
  const getStatusDisplay = (status: string) => {
    switch (status) {
      case "success":
        return { icon: CheckCircle, color: "text-green-500", bgColor: "bg-green-50 border-green-200" }
      case "denied":
        return { icon: XCircle, color: "text-red-500", bgColor: "bg-red-50 border-red-200" }
      case "timeout":
        return { icon: Clock, color: "text-yellow-500", bgColor: "bg-yellow-50 border-yellow-200" }
      case "scanning":
      case "verifying":
        return { icon: Scan, color: "text-blue-500", bgColor: "bg-blue-50 border-blue-200" }
      default:
        return { icon: AlertTriangle, color: "text-gray-500", bgColor: "bg-gray-50 border-gray-200" }
    }
  }

  return (
    <div className="min-h-screen bg-gradient-to-br from-blue-50 to-indigo-100 p-4">
      <div className="max-w-6xl mx-auto space-y-6">
        {/* Header */}
        <div className="text-center space-y-2">
          <h1 className="text-4xl font-bold text-gray-900">ESP8266 Attendance System</h1>
          <p className="text-gray-600">Fingerprint & RFID Management Interface</p>
          {isPublicTunnel && (
            <Badge variant="secondary" className="bg-green-100 text-green-800">
              <Globe className="h-3 w-3 mr-1" />
              Public Access via Tunnel
            </Badge>
          )}
        </div>

        {/* Real-time Notification */}
        {showNotification && currentNotification && attendanceMode && (
          <Card className={`border-2 ${getStatusDisplay(currentNotification.status).bgColor} animate-pulse`}>
            <CardContent className="p-6">
              <div className="flex items-center gap-4">
                {(() => {
                  const { icon: StatusIcon, color } = getStatusDisplay(currentNotification.status)
                  return <StatusIcon className={`h-12 w-12 ${color}`} />
                })()}
                <div className="flex-1">
                  <h3 className="text-2xl font-bold">{currentNotification.message}</h3>
                  <div className="space-y-1 text-sm text-gray-600">
                    <p>RFID: {currentNotification.rfidTag}</p>
                    {currentNotification.fingerprintID && <p>Fingerprint ID: #{currentNotification.fingerprintID}</p>}
                    {currentNotification.serverResponse && <p>Server: {currentNotification.serverResponse}</p>}
                  </div>
                </div>
                <Button
                  variant="outline"
                  size="sm"
                  onClick={() => {
                    setShowNotification(false)
                    clearNotification()
                  }}
                >
                  Dismiss
                </Button>
              </div>
            </CardContent>
          </Card>
        )}

        {/* Connection Card */}
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              {isConnected ? (
                isPublicTunnel ? (
                  <Globe className="h-5 w-5 text-green-500" />
                ) : (
                  <Wifi className="h-5 w-5 text-green-500" />
                )
              ) : (
                <WifiOff className="h-5 w-5 text-red-500" />
              )}
              ESP8266 Connection
            </CardTitle>
            <CardDescription>Connect to your ESP8266 device via local network or secure tunnel</CardDescription>
          </CardHeader>
          <CardContent className="space-y-4">
            {/* Ngrok Info */}
            {ngrokInfo && (
              <Alert>
                <Shield className="h-4 w-4" />
                <AlertDescription>
                  <div className="flex items-center justify-between">
                    <span>Ngrok tunnel detected: {ngrokInfo.public_url}</span>
                    <Button variant="outline" size="sm" onClick={() => setEsp8266URL(ngrokInfo.public_url)}>
                      Use This Tunnel
                    </Button>
                  </div>
                </AlertDescription>
              </Alert>
            )}

            <div className="flex gap-4 items-end">
              <div className="flex-1">
                <Label htmlFor="esp8266-url">ESP8266 URL/Address</Label>
                <Input
                  id="esp8266-url"
                  value={esp8266URL}
                  onChange={(e) => setEsp8266URL(e.target.value)}
                  placeholder="https://abc123.ngrok.io or http://192.168.43.201"
                  disabled={isLoading}
                />
                <p className="text-xs text-gray-500 mt-1">
                  Use tunnel URL for public access or local IP for private network
                </p>
              </div>
              <Button onClick={testConnection} disabled={isLoading}>
                {isLoading ? "Testing..." : "Test Connection"}
              </Button>
            </div>

            {isConnected && (
              <Alert>
                {isPublicTunnel ? <Globe className="h-4 w-4" /> : <Wifi className="h-4 w-4" />}
                <AlertDescription>
                  Successfully connected to ESP8266 via {isPublicTunnel ? "secure tunnel" : "local network"} at{" "}
                  {esp8266URL}
                  {isPublicTunnel && (
                    <div className="mt-2">
                      <Button variant="outline" size="sm" onClick={() => window.open(esp8266URL, "_blank")}>
                        <ExternalLink className="h-3 w-3 mr-1" />
                        Open in New Tab
                      </Button>
                    </div>
                  )}
                </AlertDescription>
              </Alert>
            )}
          </CardContent>
        </Card>

        {/* Control Panel */}
        {isConnected && (
          <div className="grid md:grid-cols-2 gap-6">
            {/* Fingerprint Management */}
            <Card>
              <CardHeader>
                <CardTitle className="flex items-center gap-2">
                  <Fingerprint className="h-5 w-5" />
                  Fingerprint Management
                </CardTitle>
                <CardDescription>Manage stored fingerprints on the ESP8266 sensor</CardDescription>
              </CardHeader>
              <CardContent className="space-y-4">
                <div className="flex justify-between items-center">
                  <h3 className="text-lg font-semibold">Stored Fingerprints ({fingerprints.length})</h3>
                  <div className="flex gap-2">
                    <Button onClick={loadFingerprints} variant="outline" size="sm" disabled={isLoading}>
                      Refresh
                    </Button>
                    <Dialog open={isEnrollDialogOpen} onOpenChange={setIsEnrollDialogOpen}>
                      <DialogTrigger asChild>
                        <Button size="sm" disabled={isLoading}>
                          <Plus className="h-4 w-4 mr-2" />
                          Add Fingerprint
                        </Button>
                      </DialogTrigger>
                      <DialogContent>
                        <DialogHeader>
                          <DialogTitle>Enroll New Fingerprint</DialogTitle>
                          <DialogDescription>
                            Enter an ID (1-127) for the new fingerprint. Make sure the ID is not already in use.
                          </DialogDescription>
                        </DialogHeader>
                        <div className="space-y-4">
                          <div>
                            <Label htmlFor="fingerprint-id">Fingerprint ID</Label>
                            <Input
                              id="fingerprint-id"
                              type="number"
                              min="1"
                              max="127"
                              value={newFingerprintId}
                              onChange={(e) => setNewFingerprintId(e.target.value)}
                              placeholder="Enter ID (1-127)"
                            />
                          </div>
                        </div>
                        <DialogFooter>
                          <Button variant="outline" onClick={() => setIsEnrollDialogOpen(false)}>
                            Cancel
                          </Button>
                          <Button onClick={enrollFingerprint} disabled={!newFingerprintId || isLoading}>
                            Start Enrollment
                          </Button>
                        </DialogFooter>
                      </DialogContent>
                    </Dialog>
                  </div>
                </div>

                {fingerprints.length > 0 ? (
                  <Table>
                    <TableHeader>
                      <TableRow>
                        <TableHead>ID</TableHead>
                        <TableHead>Status</TableHead>
                        <TableHead>Actions</TableHead>
                      </TableRow>
                    </TableHeader>
                    <TableBody>
                      {fingerprints.map((fp) => (
                        <TableRow key={fp.id}>
                          <TableCell className="font-medium">#{fp.id}</TableCell>
                          <TableCell>
                            <Badge variant="secondary">{fp.status}</Badge>
                          </TableCell>
                          <TableCell>
                            <Button
                              variant="destructive"
                              size="sm"
                              onClick={() => deleteFingerprint(fp.id)}
                              disabled={isLoading}
                            >
                              <Trash2 className="h-4 w-4" />
                            </Button>
                          </TableCell>
                        </TableRow>
                      ))}
                    </TableBody>
                  </Table>
                ) : (
                  <div className="text-center py-8 text-gray-500">
                    No fingerprints stored. Click "Add Fingerprint" to enroll a new one.
                  </div>
                )}
              </CardContent>
            </Card>

            {/* Attendance Mode */}
            <Card>
              <CardHeader>
                <CardTitle className="flex items-center gap-2">
                  <CreditCard className="h-5 w-5" />
                  Attendance Mode
                </CardTitle>
                <CardDescription>Start/stop attendance scanning mode</CardDescription>
              </CardHeader>
              <CardContent className="space-y-4">
                <div className="text-center space-y-4">
                  <div className="p-6 border-2 border-dashed border-gray-300 rounded-lg">
                    <Scan className="h-12 w-12 mx-auto text-gray-400 mb-4" />
                    <p className="text-lg font-semibold">
                      {attendanceMode ? "Attendance Mode Active" : "Attendance Mode Inactive"}
                    </p>
                    <p className="text-sm text-gray-600">
                      {attendanceMode
                        ? "System is ready to scan RFID cards and verify fingerprints"
                        : "Click the button below to start attendance scanning"}
                    </p>
                  </div>

                  <Button
                    onClick={toggleAttendanceMode}
                    disabled={isLoading}
                    variant={attendanceMode ? "destructive" : "default"}
                    size="lg"
                    className="w-full"
                  >
                    {attendanceMode ? "Stop Attendance Mode" : "Start Attendance Mode"}
                  </Button>
                </div>

                {/* Recent Attendance Records */}
                {recentNotifications.length > 0 && (
                  <div className="space-y-2">
                    <h4 className="font-semibold">Recent Scans</h4>
                    <div className="max-h-40 overflow-y-auto space-y-1">
                      {recentNotifications.slice(0, 5).map((record, index) => {
                        const { color, bgColor } = getStatusDisplay(record.status)
                        return (
                          <div key={index} className={`text-xs p-2 rounded border ${bgColor}`}>
                            <div className="flex justify-between">
                              <span>RFID: {record.rfidTag}</span>
                              <span className={color}>{record.message}</span>
                            </div>
                            {record.fingerprintID && (
                              <div className="text-gray-600">Fingerprint: #{record.fingerprintID}</div>
                            )}
                            <div className="text-gray-500">
                              {new Date(Number.parseInt(record.timestamp)).toLocaleTimeString()}
                            </div>
                          </div>
                        )
                      })}
                    </div>
                  </div>
                )}
              </CardContent>
            </Card>
          </div>
        )}

        {/* Tunnel Setup Instructions */}
        <Card>
          <CardHeader>
            <CardTitle>Tunnel Setup Instructions</CardTitle>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="grid md:grid-cols-2 gap-4">
              <div>
                <h4 className="font-semibold mb-2">Ngrok Setup:</h4>
                <ol className="list-decimal list-inside space-y-1 text-sm">
                  <li>
                    Install ngrok: <code className="bg-gray-100 px-1 rounded">npm install -g ngrok</code>
                  </li>
                  <li>Sign up at ngrok.com and get auth token</li>
                  <li>
                    Configure: <code className="bg-gray-100 px-1 rounded">ngrok config add-authtoken TOKEN</code>
                  </li>
                  <li>
                    Start tunnel: <code className="bg-gray-100 px-1 rounded">ngrok http 192.168.43.201:80</code>
                  </li>
                </ol>
              </div>
              <div>
                <h4 className="font-semibold mb-2">Benefits:</h4>
                <ul className="list-disc list-inside space-y-1 text-sm">
                  <li>✅ HTTPS encryption automatically</li>
                  <li>✅ No router configuration needed</li>
                  <li>✅ Works behind firewalls</li>
                  <li>✅ Easy to share with others</li>
                </ul>
              </div>
            </div>
          </CardContent>
        </Card>
      </div>
    </div>
  )
}
